#pragma once

#include <cmath>

namespace smartwater {

double greatCircleDist(double longitude1, double latitude1, double longitude2,
                       double latitude2) {
  constexpr double EARTH_RADIUS = 6378000;
  double lot1r = longitude1 * 180 / M_PI;
  double lat1r = latitude1 * 180 / M_PI;

  double lot2r = longitude2 * 180 / M_PI;
  double lat2r = latitude2 * 180 / M_PI;

  double p1x = sin(lot1r) * EARTH_RADIUS;
  double p1y = cos(lot1r) * EARTH_RADIUS;
  double p1z = sin(lat1r) * EARTH_RADIUS;

  double p2x = sin(lot2r) * EARTH_RADIUS;
  double p2y = cos(lot2r) * EARTH_RADIUS;
  double p2z = sin(lat2r) * EARTH_RADIUS;

  double dp = p1x * p2x + p1y * p2y + p1z * p2z;
  return EARTH_RADIUS * acos(dp / (EARTH_RADIUS * EARTH_RADIUS));
}

} // namespace smartwater
