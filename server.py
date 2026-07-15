"""
MPPT Solar Monitor — FastAPI Backend
Serves the dashboard HTML and exposes REST endpoints for data ingestion/retrieval.
"""

from __future__ import annotations

import datetime
from collections import deque
from pathlib import Path
from typing import Optional, Dict, Any

from fastapi import FastAPI, Request, HTTPException, UploadFile, File
from fastapi.responses import HTMLResponse
from fastapi.templating import Jinja2Templates
from pydantic import BaseModel
import uvicorn
import httpx

# ---------------------------------------------------------------------------
# App & templates
# ---------------------------------------------------------------------------
app = FastAPI(title="MPPT Solar Monitor")

BASE_DIR = Path(__file__).resolve().parent
templates = Jinja2Templates(directory=str(BASE_DIR / "templates"))

# ---------------------------------------------------------------------------
# In-memory data store  (thread-safe enough for a single-worker uvicorn)
# ---------------------------------------------------------------------------
MAX_HISTORY = 200  # keep last 200 readings; frontend asks for 30

history: deque[dict] = deque(maxlen=MAX_HISTORY)
latest: dict = {}

# Store pending settings to send to ESP32 on its next ping
pending_settings: Optional[Dict[str, int]] = None
pending_load_cmd: Optional[bool] = None

# Store the IP of the last device to send data
esp32_ip: Optional[str] = None

# ---------------------------------------------------------------------------
# Pydantic models
# ---------------------------------------------------------------------------

class SensorPayload(BaseModel):
    v_plts: Optional[float] = 0.0
    i_plts: Optional[float] = 0.0
    batt_pct: Optional[float] = 0.0
    v_out: Optional[float] = 0.0
    i_out: Optional[float] = 0.0
    # New configuration fields from ESP32
    sol_vmax: Optional[int] = 18
    sol_imax: Optional[int] = 5
    bat_type: Optional[int] = 0
    sys_volt: Optional[int] = 12
    bat_cap: Optional[int] = 50
    load_status: Optional[bool] = False

class TogglePayload(BaseModel):
    state: bool

class SettingsPayload(BaseModel):
    sol_vmax: int
    sol_imax: int
    bat_type: int
    sys_volt: int
    bat_cap: int

class LoginPayload(BaseModel):
    username: str
    password: str

# ---------------------------------------------------------------------------
# Routes
# ---------------------------------------------------------------------------

@app.get("/", response_class=HTMLResponse)
async def index(request: Request):
    """Serve the main dashboard page."""
    return templates.TemplateResponse(request=request, name="index.html")

@app.post("/api/login")
async def login(payload: LoginPayload):
    """Simple login endpoint"""
    if payload.username == "fathul" and payload.password == "fathul":
        return {"status": "ok", "token": "fathul_admin_token_2026"}
    raise HTTPException(status_code=401, detail="Username atau Password salah")

@app.post("/api/data")
async def receive_data(payload: SensorPayload, request: Request):
    """Accept sensor data from ESP32 (or simulator)."""
    global latest, pending_settings, esp32_ip, pending_load_cmd
    
    if request.client and request.client.host:
        esp32_ip = request.client.host

    v_plts = payload.v_plts or 0.0
    i_plts = payload.i_plts or 0.0
    batt_pct = payload.batt_pct or 0.0
    v_out = payload.v_out or 0.0
    i_out = payload.i_out or 0.0

    p_plts = round(v_plts * i_plts, 2)
    p_out = round(v_out * i_out, 2)
    efficiency = round((p_out / p_plts * 100), 1) if p_plts > 0 else 0.0

    record = {
        "v_plts": round(v_plts, 2),
        "i_plts": round(i_plts, 2),
        "p_plts": p_plts,
        "batt_pct": round(batt_pct, 1),
        "v_out": round(v_out, 2),
        "i_out": round(i_out, 2),
        "p_out": p_out,
        "efficiency": efficiency,
        "timestamp": datetime.datetime.now().isoformat(timespec="seconds"),
        # Configuration parameters
        "sol_vmax": payload.sol_vmax,
        "sol_imax": payload.sol_imax,
        "bat_type": payload.bat_type,
        "sys_volt": payload.sys_volt,
        "bat_cap": payload.bat_cap,
        "load_status": payload.load_status,
    }

    latest = record
    history.append(record)

    response = {"status": "ok", "received": True}
    
    # If there are pending settings, append them to the response for the ESP32
    if pending_settings is not None:
        response["update_settings"] = True
        response["settings"] = pending_settings
        print(f"Sent pending settings to ESP32: {response['settings']}")
        pending_settings = None # Clear after sending
        
    if pending_load_cmd is not None:
        response["remote_load_cmd"] = pending_load_cmd
        print(f"Sent remote_load_cmd to ESP32: {pending_load_cmd}")
        pending_load_cmd = None

    return response


@app.get("/api/latest")
async def get_latest():
    """Return the most recent data point with computed power values."""
    if not latest:
        return {"status": "no_data"}
        
    # Compute the age of the data point directly on the server to avoid timezone issues
    last_update = datetime.datetime.fromisoformat(latest["timestamp"])
    age = (datetime.datetime.now() - last_update).total_seconds()
    latest["age_seconds"] = age
    
    return {"status": "ok", "data": latest}


@app.get("/api/history")
async def get_history(n: int = 30):
    """Return the last *n* data points (default 30) for charting."""
    n = min(n, len(history))
    items = list(history)[-n:]
    return {"status": "ok", "count": len(items), "data": items}


@app.post("/api/settings")
async def update_settings(payload: SettingsPayload):
    """Queue settings update to be sent to ESP32 on its next POST."""
    global pending_settings
    pending_settings = {
        "sol_vmax": payload.sol_vmax,
        "sol_imax": payload.sol_imax,
        "bat_type": payload.bat_type,
        "sys_volt": payload.sys_volt,
        "bat_cap": payload.bat_cap
    }
    return {"status": "ok", "message": "Pengaturan disimpan dan menunggu sinkronisasi dengan ESP32..."}

@app.post("/api/toggle_load")
async def toggle_load(payload: TogglePayload):
    """Queue a load toggle command for the ESP32."""
    global pending_load_cmd
    pending_load_cmd = payload.state
    return {"status": "ok", "message": "Command queued for ESP32"}

@app.post("/api/ota")
async def handle_ota(file: UploadFile = File(...)):
    """Forward the uploaded .bin file to the ESP32 for OTA updating."""
    if not esp32_ip:
        return {"status": "error", "message": "IP ESP32 belum terdeteksi. Pastikan ESP32 sudah mengirim data minimal 1 kali."}
    
    try:
        file_bytes = await file.read()
        url = f"http://{esp32_ip}/update"
        
        # Match standard ESP32 WebUpdate multipart/form-data requirements
        files_payload = {'update': (file.filename, file_bytes, 'application/octet-stream')}
        
        async with httpx.AsyncClient() as client:
            # We use a long timeout since flash write operations can be slow
            response = await client.post(url, files=files_payload, timeout=120.0)
        
        if response.status_code == 200:
            return {"status": "success", "message": "✅ UPDATE SUKSES! ESP32 sedang Restart..."}
        else:
            return {"status": "error", "message": f"❌ Gagal! ESP32 menolak. Kode Status: {response.status_code}"}
    except Exception as e:
        return {"status": "error", "message": f"⚠️ Error Koneksi ke ESP32: {str(e)}"}


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    uvicorn.run("server:app", host="0.0.0.0", port=8000, reload=True)
