#!/usr/bin/env python3
# =====================================================================
#  download_icgc_tiles.py
#
#  Downloads ICGC topographic map tiles for offline use in Processing.
#  Supports multiple coverage zones. Tiles that appear in more than
#  one zone are downloaded only once.
#
#  Output layout (sketch's data/ folder):
#      tiles/{z}/{x}/{y}.png
#
#  Run once from the Processing sketch folder:
#      py download_icgc_tiles.py        (Windows)
#      python3 download_icgc_tiles.py   (Linux / macOS)
#
#  Source:  ICGC Base Map Service, "topografic" layer
#           License: CC-BY (Catalonia) / ODbL (rest of world)
#           https://www.icgc.cat/en/Geoinformation-and-Maps/Base-Map-Service
#
#  Slippy-map tile math: OpenStreetMap Wiki, "Slippy map tilenames"
# =====================================================================

import os
import math
import time
import urllib.request
import urllib.error


# ---------------------------------------------------------------------
#  CONFIGURATION
# ---------------------------------------------------------------------

# List of coverage zones. Each entry: (name, lat, lon, radius_km)
# Add or remove zones freely; tiles shared between zones are only
# downloaded once.
ZONES = [
    ("Terrassa (ESEIAAT)", 41.5638, 2.0222, 20.0),
    ("Piera",              41.5230, 1.7528,  20.0),
]

# Zoom levels to cache
#   z=12 ~29 m/px (city overview)    z=15 ~3.6 m/px (street detail)
#   z=13 ~14 m/px (neighborhoods)    z=16 ~1.8 m/px (building detail)
#   z=14 ~7 m/px  (blocks)
ZOOM_MIN = 8
ZOOM_MAX = 17

# ICGC Base Map Service - topographic layer
TILE_URL = ("https://geoserveis.icgc.cat/servei/catalunya/mapa-base/"
            "wmts/topografic/MON3857NW/{z}/{x}/{y}.png")

OUTPUT_DIR = os.path.join("data", "tiles")

# Politeness: max ~3 requests per second
DELAY_SEC = 0.3

USER_AGENT = "TFG-AvionicsInstrumentation/1.0 (academic ESEIAAT-UPC)"


# ---------------------------------------------------------------------
#  SLIPPY MAP TILE MATH
# ---------------------------------------------------------------------

def latlon_to_tile(lat, lon, z):
    """Web Mercator tile index (x, y) for a given lat/lon and zoom."""
    lat_rad = math.radians(lat)
    n = 2 ** z
    x = int((lon + 180.0) / 360.0 * n)
    y = int((1.0 - math.log(math.tan(lat_rad) + 1.0 / math.cos(lat_rad))
            / math.pi) / 2.0 * n)
    return x, y


def bbox_km(lat, lon, km):
    """Return (south, north, west, east) around lat/lon with radius km."""
    dlat = km / 111.0
    dlon = km / (111.0 * math.cos(math.radians(lat)))
    return lat - dlat, lat + dlat, lon - dlon, lon + dlon


# ---------------------------------------------------------------------
#  DOWNLOAD
# ---------------------------------------------------------------------

def download_tile(z, x, y, out_path):
    """Download one tile. Returns 'ok', 'skipped', 'missing' or 'fail'."""
    if os.path.exists(out_path):
        return "skipped"

    url = TILE_URL.format(z=z, x=x, y=y)
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})

    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            data = resp.read()
    except urllib.error.HTTPError as e:
        if e.code == 404:
            return "missing"
        print("  HTTP {} at {}/{}/{}".format(e.code, z, x, y))
        return "fail"
    except Exception as e:
        print("  ERROR at {}/{}/{}: {}".format(z, x, y, e))
        return "fail"

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(data)
    return "ok"


# ---------------------------------------------------------------------
#  MAIN
# ---------------------------------------------------------------------

def main():
    print("=" * 60)
    print("ICGC Topographic Tile Downloader (multi-zone)")
    print("=" * 60)
    print("Zoom    : {} .. {}".format(ZOOM_MIN, ZOOM_MAX))
    print("Output  : {}".format(OUTPUT_DIR))
    print("Source  : ICGC Base Map Service (topografic)")
    print()

    # Build the job set. A set deduplicates tiles shared between zones.
    jobs = set()
    print("Zones:")
    for name, lat, lon, radius in ZONES:
        south, north, west, east = bbox_km(lat, lon, radius)
        zone_count = 0

        for z in range(ZOOM_MIN, ZOOM_MAX + 1):
            x_min, y_max = latlon_to_tile(south, west, z)
            x_max, y_min = latlon_to_tile(north, east, z)
            for x in range(x_min, x_max + 1):
                for y in range(y_min, y_max + 1):
                    jobs.add((z, x, y))
                    zone_count += 1

        print("  {:24s} {:.4f}N {:.4f}E  r={:.0f}km  {:6d} tiles"
              .format(name, lat, lon, radius, zone_count))

    # Sort by zoom, then x, then y for a predictable download order
    jobs = sorted(jobs)
    total = len(jobs)
    est_mb = total * 20 / 1024.0
    est_min = total * DELAY_SEC / 60.0

    print()
    print("Unique tiles (deduplicated) : {}".format(total))
    print("Estimated size              : ~{:.0f} MB".format(est_mb))
    print("Estimated time              : ~{:.1f} min".format(est_min))
    print()
    print("Starting in 3 seconds... (Ctrl+C to cancel)")

    try:
        for i in range(3, 0, -1):
            print("  {}...".format(i))
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nCancelled.")
        return

    print()
    ok_count = 0
    skipped_count = 0
    fail_count = 0
    missing_count = 0
    start_time = time.time()

    try:
        for i, (z, x, y) in enumerate(jobs):
            out_path = os.path.join(OUTPUT_DIR, str(z), str(x),
                                    "{}.png".format(y))
            result = download_tile(z, x, y, out_path)

            if result == "ok":
                ok_count += 1
                time.sleep(DELAY_SEC)
            elif result == "skipped":
                skipped_count += 1
            elif result == "missing":
                missing_count += 1
                time.sleep(DELAY_SEC)
            else:
                fail_count += 1
                time.sleep(DELAY_SEC)

            if (i + 1) % 100 == 0:
                elapsed = time.time() - start_time
                rate = (i + 1) / elapsed if elapsed > 0 else 0
                remaining = (total - i - 1) / rate / 60.0 if rate > 0 else 0
                print("  [{}/{}]  {:.1f} tiles/s  ETA {:.1f} min"
                      .format(i + 1, total, rate, remaining))

    except KeyboardInterrupt:
        print("\n\nInterrupted. Progress so far:")

    print()
    print("=" * 60)
    print("Downloaded : {}".format(ok_count))
    print("Skipped    : {} (already on disk)".format(skipped_count))
    print("Missing    : {} (404 from server)".format(missing_count))
    print("Failed     : {}".format(fail_count))
    print("=" * 60)

    if ok_count > 0 or skipped_count > 0:
        print("\nTiles saved under '{}/'".format(OUTPUT_DIR))


if __name__ == "__main__":
    main()