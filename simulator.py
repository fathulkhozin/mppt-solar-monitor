"""
MPPT Solar Simulator — Sends realistic solar data to the FastAPI backend.
Simulates an ESP32 posting sensor readings and configurations every 3 seconds.
"""

import math
import random
import time

import requests

API_URL = "http://localhost:8000/api/data"

# ---------------------------------------------------------------------------
# Simulation state
# ---------------------------------------------------------------------------
battery_pct = 50.0          # starts at 50 %
time_of_day = 8.0           # virtual hour (8 AM start)
HOUR_STEP = 0.05            # each tick advances ~3 virtual minutes

# Simulated internal configurations
device_config = {
    "sol_vmax": 18,
    "sol_imax": 5,
    "bat_type": 0,
    "sys_volt": 12,
    "bat_cap": 50
}


def solar_irradiance(hour: float) -> float:
    """Return a 0-1 irradiance factor based on time of day (bell curve)."""
    # Peak at noon (12), sunrise ~6, sunset ~18
    if hour < 5.5 or hour > 18.5:
        return 0.0
    return max(0.0, math.sin(math.pi * (hour - 5.5) / 13.0))


def generate_reading(hour: float, batt: float) -> dict:
    irr = solar_irradiance(hour)
    # Add realistic noise
    irr *= random.uniform(0.85, 1.05)   # cloud flicker
    irr = max(irr, 0.0)

    v_plts = 15.0 + 6.0 * irr + random.uniform(-0.3, 0.3)
    i_plts = 5.0 * irr + random.uniform(-0.1, 0.1)
    i_plts = max(i_plts, 0.0)

    v_out = 12.0 + 2.4 * irr + random.uniform(-0.15, 0.15)
    i_out = 6.0 * irr * random.uniform(0.80, 1.0) + random.uniform(-0.05, 0.05)
    i_out = max(i_out, 0.0)

    # Battery slowly charges when there's sun, slowly drains at night
    p_in = v_plts * i_plts
    p_out = v_out * i_out
    delta = (p_in - p_out) * 0.005  # tiny charge delta
    batt = min(100.0, max(0.0, batt + delta + random.uniform(-0.1, 0.1)))

    reading = {
        "v_plts": round(v_plts, 2),
        "i_plts": round(i_plts, 2),
        "batt_pct": round(batt, 1),
        "v_out": round(v_out, 2),
        "i_out": round(i_out, 2),
    }
    # Merge reading with current device configuration
    reading.update(device_config)
    return reading, batt


def main():
    global battery_pct, time_of_day, device_config
    print("==================================================")
    print("    Solar Simulator & Settings Sync                   ")
    print("    Posting to", API_URL, "      ")
    print("==================================================")

    while True:
        data, battery_pct = generate_reading(time_of_day, battery_pct)
        try:
            resp = requests.post(API_URL, json=data, timeout=2)
            ts = time.strftime('%H:%M:%S')
            virt = f"{int(time_of_day):02d}:{int((time_of_day % 1) * 60):02d}"
            
            # Check for settings update request from server
            if resp.status_code == 200:
                resp_json = resp.json()
                if resp_json.get("update_settings"):
                    new_settings = resp_json.get("settings", {})
                    print(f"\n[!] OVER THE AIR UPDATE RECEIVED:")
                    print(f"    Current: {device_config}")
                    print(f"    New    : {new_settings}")
                    device_config.update(new_settings)
                    print(f"    -> Applied successfully!\n")

            print(
                f"[{ts}] vHour={virt}  "
                f"V_plts={data['v_plts']:5.1f}V  I_plts={data['i_plts']:4.2f}A  "
                f"Batt={data['batt_pct']:5.1f}%  "
                f"-> {resp.status_code}"
            )
        except requests.ConnectionError:
            print(f"[{time.strftime('%H:%M:%S')}]  !  Server not reachable - retrying ...")

        time_of_day += HOUR_STEP
        if time_of_day >= 24.0:
            time_of_day = 0.0

        time.sleep(3)


if __name__ == "__main__":
    main()
