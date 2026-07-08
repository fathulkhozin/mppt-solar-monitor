.\venv\Scripts\python.exe server.pyimport reflex as rx
import time
from typing import List, Dict
from fastapi import FastAPI, Request

# Initialize FastAPI app for custom routes
fastapi_app = FastAPI()

# Global variable for latest data (for fast UI updates)
latest_data = {
    "v_plts": 0.0,
    "i_plts": 0.0,
    "batt_pct": 0.0,
    "v_out": 0.0,
    "i_out": 0.0,
    "p_plts": 0.0,
    "p_out": 0.0,
}

# Global history in memory (max 100 points)
global_history = []

# FastAPI Endpoint for ESP32
@fastapi_app.post("/api/data")
async def api_data(request: Request):
    data = await request.json()
    v_plts = float(data.get("v_plts", 0))
    i_plts = float(data.get("i_plts", 0))
    batt_pct = float(data.get("batt_pct", 0))
    v_out = float(data.get("v_out", 0))
    i_out = float(data.get("i_out", 0))
    
    global latest_data, global_history
    latest_data.update({
        "v_plts": v_plts,
        "i_plts": i_plts,
        "batt_pct": batt_pct,
        "v_out": v_out,
        "i_out": i_out,
        "p_plts": round(v_plts * i_plts, 2),
        "p_out": round(v_out * i_out, 2),
    })
    
    # Save to history
    t_str = time.strftime("%H:%M:%S", time.localtime())
    global_history.append({
        "time": t_str,
        "Daya PLTS (W)": round(v_plts * i_plts, 2),
        "Daya Output (W)": round(v_out * i_out, 2),
    })
    
    # Keep only last 20 points for UI performance
    if len(global_history) > 20:
        global_history.pop(0)
        
    return {"status": "ok"}

# State for Dashboard
class DashboardState(rx.State):
    v_plts: float = 0.0
    i_plts: float = 0.0
    batt_pct: float = 0.0
    v_out: float = 0.0
    i_out: float = 0.0
    p_plts: float = 0.0
    p_out: float = 0.0
    
    # Graph data
    history_data: List[Dict[str, str | float]] = []
    
    @rx.event(background=True)
    async def update_dashboard(self):
        while True:
            async with self:
                self.v_plts = latest_data["v_plts"]
                self.i_plts = latest_data["i_plts"]
                self.batt_pct = latest_data["batt_pct"]
                self.v_out = latest_data["v_out"]
                self.i_out = latest_data["i_out"]
                self.p_plts = latest_data["p_plts"]
                self.p_out = latest_data["p_out"]
                self.history_data = list(global_history)
                
            import asyncio
            await asyncio.sleep(2) # Update UI every 2 seconds

def stat_card(title: str, value: str, icon: str, color: str):
    return rx.card(
        rx.hstack(
            rx.icon(tag=icon, size=40, color=color),
            rx.vstack(
                rx.text(title, font_weight="bold", color="gray", size="2"),
                rx.text(value, font_size="2em", font_weight="bold"),
                align_items="start",
            ),
            spacing="4",
        ),
        width="100%",
        padding="1.5em",
        border_radius="10px",
        box_shadow="0 4px 6px rgba(0,0,0,0.1)",
    )

def index() -> rx.Component:
    return rx.container(
        rx.vstack(
            rx.heading("Monitor MPPT PLTS Real-Time", font_size="2.5em", margin_bottom="1em", color="teal"),
            
            # Grid for Stats Cards
            rx.grid(
                stat_card("Tegangan PLTS", f"{DashboardState.v_plts} V", "sun", "orange"),
                stat_card("Arus PLTS", f"{DashboardState.i_plts} A", "zap", "orange"),
                stat_card("Daya PLTS", f"{DashboardState.p_plts} W", "flame", "red"),
                
                stat_card("Kapasitas Baterai", f"{DashboardState.batt_pct} %", "battery-charging", "green"),
                
                stat_card("Tegangan Output", f"{DashboardState.v_out} V", "plug", "blue"),
                stat_card("Arus Output", f"{DashboardState.i_out} A", "activity", "blue"),
                stat_card("Daya Output", f"{DashboardState.p_out} W", "zap", "purple"),
                
                columns="3",
                spacing="4",
                width="100%",
                margin_bottom="2em",
            ),
            
            # Graph
            rx.card(
                rx.vstack(
                    rx.heading("Grafik Daya (PLTS vs Output)", size="4", margin_bottom="1em"),
                    rx.recharts.line_chart(
                        rx.recharts.line(
                            data_key="Daya PLTS (W)",
                            stroke="#ff7300",
                        ),
                        rx.recharts.line(
                            data_key="Daya Output (W)",
                            stroke="#387908",
                        ),
                        rx.recharts.x_axis(data_key="time"),
                        rx.recharts.y_axis(),
                        rx.recharts.cartesian_grid(stroke_dasharray="3 3"),
                        rx.recharts.graphing_tooltip(),
                        rx.recharts.legend(),
                        data=DashboardState.history_data,
                        height=400,
                        width="100%",
                    ),
                    width="100%",
                ),
                width="100%",
                padding="2em",
                box_shadow="0 4px 6px rgba(0,0,0,0.1)",
                border_radius="10px",
            ),
            width="100%",
            align_items="center",
        ),
        padding="2em",
        max_width="1200px",
        on_mount=DashboardState.update_dashboard,
    )

app = rx.App(api_transformer=fastapi_app)
app.add_page(index)
