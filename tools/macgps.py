"""Optional Mac location for `obdlog.py --gps`, through CoreLocation (pyobjc).

Install with `pip install -r tools/requirements-gps.txt`. macOS asks once whether the
terminal app may use location; if it is refused or the library is missing, `start` raises
GpsUnavailable and the logger carries on without GPS. Untested on a real drive so far.
"""


class GpsUnavailable(Exception):
    pass


def start():
    """Start location updates. Returns an object whose poll() gives new fixes as dicts."""
    try:
        import CoreLocation  # noqa: PLC0415 - optional dependency, imported on demand
        from Foundation import (NSDate, NSDefaultRunLoopMode, NSObject,  # noqa: PLC0415
                                NSRunLoop)
    except ImportError as e:
        raise GpsUnavailable(f"pyobjc CoreLocation not installed: {e}") from e

    if not CoreLocation.CLLocationManager.locationServicesEnabled():
        raise GpsUnavailable("location services are off in System Settings")

    class Delegate(NSObject):
        def init(self):
            self = super().init()
            self.fixes, self.error = [], None
            return self

        def locationManager_didUpdateLocations_(self, _mgr, locations):
            for loc in locations:
                c = loc.coordinate()
                self.fixes.append({
                    "lat": c.latitude, "lon": c.longitude, "alt": loc.altitude(),
                    "h_acc": loc.horizontalAccuracy(), "speed_mps": loc.speed(),
                    "course": loc.course(),
                    "t_fix": loc.timestamp().timeIntervalSince1970(),
                })

        def locationManager_didFailWithError_(self, _mgr, err):
            self.error = str(err)

    class Source:
        def __init__(self):
            self.delegate = Delegate.alloc().init()
            self.mgr = CoreLocation.CLLocationManager.alloc().init()
            self.mgr.setDelegate_(self.delegate)
            self.mgr.setDesiredAccuracy_(CoreLocation.kCLLocationAccuracyBest)
            self.mgr.requestWhenInUseAuthorization()
            self.mgr.startUpdatingLocation()

        def poll(self):
            """Handle pending location events without waiting; return (fixes, error)."""
            NSRunLoop.currentRunLoop().runMode_beforeDate_(NSDefaultRunLoopMode,
                                                           NSDate.distantPast())
            fixes, self.delegate.fixes = self.delegate.fixes, []
            err, self.delegate.error = self.delegate.error, None
            return fixes, err

        def stop(self):
            self.mgr.stopUpdatingLocation()

    return Source()
