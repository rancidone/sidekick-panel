"""Real-world weather and daylight, emitted as periodic ``weather`` events.

Sun position (solar.py) needs no network and is recomputed every tick.
Actual conditions — cloud cover, fog, precipitation, wind — come from
Open-Meteo (https://open-meteo.com/), a free forecast API that requires no
API key and no signup, refreshed on a slower cadence since they don't change
minute to minute. Network failures are best-effort: the last known
conditions (or a calm default) are reused rather than blocking the tick.
"""

from __future__ import annotations

import json
import time
import urllib.error
import urllib.request
from dataclasses import dataclass

from magicpanel.desktop.emit import emit
from magicpanel.desktop.solar import daylight_fields

# Las Vegas, NV.
DEFAULT_LAT = 36.1699
DEFAULT_LON = -115.1398

DEFAULT_CONDITIONS_INTERVAL = 600.0  # how often to poll Open-Meteo
DEFAULT_SOLAR_INTERVAL = 60.0  # how often to recompute sun position + emit

_FORECAST_URL = "https://api.open-meteo.com/v1/forecast"
_REQUEST_TIMEOUT = 10.0

# WMO weather codes (Open-Meteo's `weather_code`) grouped into the engine's
# precip vocabulary. Anything not listed below is treated as no precip.
_THUNDERSTORM_CODES = {95, 96, 99}
_RAIN_CODES = {51, 53, 55, 56, 57, 61, 63, 65, 66, 67, 80, 81, 82}
_SNOW_CODES = {71, 73, 75, 77, 85, 86}
_FOG_CODES = {45, 48}


@dataclass(frozen=True)
class Conditions:
    clouds: str
    fog: str
    weather: str
    wind: int


CALM_CONDITIONS = Conditions(clouds="clear", fog="none", weather="none", wind=40)


def _cloud_cover_bucket(percent: float) -> str:
    if percent <= 20.0:
        return "clear"
    if percent <= 70.0:
        return "light"
    return "dense"


def _weather_code_to_precip(code: int) -> str:
    if code in _THUNDERSTORM_CODES:
        return "thunderstorm"
    if code in _RAIN_CODES:
        return "rain"
    if code in _SNOW_CODES:
        return "snow"
    return "none"


def fetch_conditions(
    lat: float = DEFAULT_LAT, lon: float = DEFAULT_LON, timeout: float = _REQUEST_TIMEOUT
) -> Conditions:
    """Fetch current conditions from Open-Meteo. Raises on network/parse failure."""
    params = (
        f"latitude={lat}&longitude={lon}"
        "&current=cloud_cover,wind_speed_10m,weather_code"
        "&wind_speed_unit=kmh&timezone=UTC"
    )
    url = f"{_FORECAST_URL}?{params}"
    with urllib.request.urlopen(url, timeout=timeout) as response:
        payload = json.loads(response.read().decode("utf-8"))
    current = payload["current"]

    code = int(current["weather_code"])
    wind_kmh = float(current["wind_speed_10m"])

    return Conditions(
        clouds=_cloud_cover_bucket(float(current["cloud_cover"])),
        fog="dense" if code in _FOG_CODES else "none",
        weather=_weather_code_to_precip(code),
        wind=max(0, min(255, round(wind_kmh * 1.8))),
    )


def run_weather(
    lat: float = DEFAULT_LAT,
    lon: float = DEFAULT_LON,
    conditions_interval: float = DEFAULT_CONDITIONS_INTERVAL,
    solar_interval: float = DEFAULT_SOLAR_INTERVAL,
    iterations: int | None = None,
) -> None:
    """Loop: emit a ``weather`` event every ``solar_interval`` seconds with a
    fresh sun position, refreshing real conditions every ``conditions_interval``
    seconds (best-effort — keeps the last known conditions on fetch failure).
    """
    conditions = CALM_CONDITIONS
    ticks_per_refresh = max(1, round(conditions_interval / solar_interval))
    count = 0
    while iterations is None or count < iterations:
        if count % ticks_per_refresh == 0:
            try:
                conditions = fetch_conditions(lat, lon)
            except (urllib.error.URLError, OSError, ValueError, KeyError, TimeoutError):
                pass  # keep the last known (or calm default) conditions

        solar = daylight_fields(lat, lon)
        emit(
            "weather",
            sky=solar.sky,
            daylight=solar.daylight,
            moon=solar.moon,
            clouds=conditions.clouds,
            fog=conditions.fog,
            weather=conditions.weather,
            wind=conditions.wind,
        )

        count += 1
        if iterations is not None and count >= iterations:
            break
        time.sleep(solar_interval)
