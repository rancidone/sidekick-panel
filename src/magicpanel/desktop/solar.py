"""Real sun position, no network required.

A simplified version of NOAA's solar position algorithm: given a latitude,
longitude, and UTC time, estimate the sun's elevation above the horizon.
That elevation drives the engine's daylight/sky/moon fields directly, so
day/night and dawn/dusk twilight track the actual sun rather than a fixed
demo clock.
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from datetime import datetime, timezone

# Elevation band treated as twilight: below -6 degrees is full night (civil
# twilight ends there), above +6 degrees is full day. Between the two,
# daylight fades linearly, giving a soft dawn/dusk transition instead of a
# hard cut.
_TWILIGHT_BAND_DEGREES = 6.0


@dataclass(frozen=True)
class SolarFields:
    daylight: int  # 0-255, engine's AtmosphereConfig.daylight byte
    sky: str  # "day" or "night"
    moon: str  # "sun" or "moon"


def sun_elevation_degrees(lat: float, lon: float, when_utc: datetime) -> float:
    """Sun elevation above the horizon, in degrees. Negative = below horizon."""
    day_of_year = when_utc.timetuple().tm_yday
    hour = when_utc.hour + when_utc.minute / 60.0 + when_utc.second / 3600.0

    gamma = 2.0 * math.pi / 365.0 * (day_of_year - 1 + (hour - 12.0) / 24.0)

    eq_time = 229.18 * (
        0.000075
        + 0.001868 * math.cos(gamma)
        - 0.032077 * math.sin(gamma)
        - 0.014615 * math.cos(2 * gamma)
        - 0.040849 * math.sin(2 * gamma)
    )
    decl = (
        0.006918
        - 0.399912 * math.cos(gamma)
        + 0.070257 * math.sin(gamma)
        - 0.006758 * math.cos(2 * gamma)
        + 0.000907 * math.sin(2 * gamma)
        - 0.002697 * math.cos(3 * gamma)
        + 0.00148 * math.sin(3 * gamma)
    )

    utc_minutes = hour * 60.0
    true_solar_time = (utc_minutes + eq_time + 4.0 * lon) % 1440.0
    hour_angle_deg = true_solar_time / 4.0 - 180.0
    hour_angle = math.radians(hour_angle_deg)

    lat_rad = math.radians(lat)
    zenith = math.acos(
        math.sin(lat_rad) * math.sin(decl)
        + math.cos(lat_rad) * math.cos(decl) * math.cos(hour_angle)
    )
    return 90.0 - math.degrees(zenith)


def daylight_fields(lat: float, lon: float, when_utc: datetime | None = None) -> SolarFields:
    """Real daylight/sky/moon fields for the given location right now."""
    when_utc = when_utc or datetime.now(timezone.utc)
    elevation = sun_elevation_degrees(lat, lon, when_utc)

    amount = (elevation + _TWILIGHT_BAND_DEGREES) / (2.0 * _TWILIGHT_BAND_DEGREES)
    amount = max(0.0, min(1.0, amount))

    return SolarFields(
        daylight=round(amount * 255.0),
        sky="day" if elevation > 0.0 else "night",
        moon="sun" if elevation > 0.0 else "moon",
    )
