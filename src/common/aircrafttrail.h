/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#ifndef LITTLENAVMAP_AIRCRAFTTRACK_H
#define LITTLENAVMAP_AIRCRAFTTRACK_H

#include "geo/pos.h"

namespace Marble {
class GeoDataLatLonAltBox;
}
namespace map {
class AircraftTrailSegment;
}
namespace atools {
namespace fs {

namespace pln {
class Flightplan;
}

namespace gpx {
class GpxData;
}
namespace sc {
class SimConnectUserAircraft;
}
}
namespace geo {
class Rect;
class LineString;
}
}

namespace at {
/* User aircraft trail position. Can be converted to QVariant and thus be saved to settings.
 *
 * An invalid position indicates a break of the tracks.
 * Aircraft warp to destination above ground is not broken but a movement on ground or airport change is.
 * Done to avoid format changes. */
struct AircraftTrailPos
{
  AircraftTrailPos()
  {
  }

  AircraftTrailPos(atools::geo::PosD position, qint64 timeMs, bool ground)
    : pos(position), timestampMs(timeMs), onGround(ground)
  {
  }

  /* Create an invalid position which is used as a marker for separate trail segments */
  AircraftTrailPos(qint64 timeMs, bool ground)
    : timestampMs(timeMs), onGround(ground)
  {
  }

  bool isValid() const
  {
    return pos.isValid();
  }

  /* Get single old precision coordinate */
  atools::geo::Pos getPosition() const
  {
    return pos.asPos();
  }

  /* Get full precision coordinate */
  const atools::geo::PosD& getPosD() const
  {
    return pos;
  }

  double getAltitudeD() const
  {
    return pos.getAltitude();
  }

  /* Milliseconds since Epoch UTC */
  qint64 getTimestampMs() const
  {
    return timestampMs;
  }

  bool isOnGround() const
  {
    return onGround;
  }

private:
  friend QDataStream& operator<<(QDataStream& dataStream, const at::AircraftTrailPos& trackPos);
  friend QDataStream& operator>>(QDataStream& dataStream, at::AircraftTrailPos& trackPos);

  atools::geo::PosD pos;
  qint64 timestampMs;
  bool onGround;
};

QDataStream& operator>>(QDataStream& dataStream, at::AircraftTrailPos& obj);
QDataStream& operator<<(QDataStream& dataStream, const at::AircraftTrailPos& obj);

}

Q_DECLARE_TYPEINFO(at::AircraftTrailPos, Q_PRIMITIVE_TYPE);
Q_DECLARE_METATYPE(at::AircraftTrailPos);

/*
 * Stores the trail points of the flight simulator user aircraft.
 *
 * Points where the track is interrupted (new flight) are indicated by invalid coordinates.
 * Warping at altitude does not interrupt a track.
 */
class AircraftTrail :
  private QList<at::AircraftTrailPos>
{
public:
  AircraftTrail();
  ~AircraftTrail();

  AircraftTrail(const AircraftTrail& other);
  AircraftTrail& operator=(const AircraftTrail& other);

  /* Callback to convert lat/lon to x/y screen coordinates in findNearest() */
  typedef std::function<void (float lon, float lat, double& x, double& y)> ScreenCoordinatesFunc;

  /* Find one nearest track segment. Screen point is equal to global position and used to check maxScreenDistance. */
  map::AircraftTrailSegment findNearest(const QPoint& point, const atools::geo::Pos& position, int maxScreenDistance,
                                        const Marble::GeoDataLatLonAltBox& viewportBox, ScreenCoordinatesFunc screenCoordinates) const;

  /* Build a GpxData from this object and assigns flight plan to it. */
  atools::fs::gpx::GpxData toGpxData(const atools::fs::pln::Flightplan& flightplan) const;

  /* Erases trail and populates this with the given gpxData */
  void fillTrailFromGpxData(const atools::fs::gpx::GpxData& gpxData);

  /* Saves and restores track into a separate file (little_navmap.track). Creates two additional backup files. */
  void saveState(const QString& suffix, int numBackupFiles);
  void restoreState(const QString& suffix);

  void clearTrail();

  /*
   * Add a track position. Accurracy depends on the ground flag which will cause more
   * or less points skipped.
   * @return true if the track was pruned
   */
  bool appendTrailPos(const atools::fs::sc::SimConnectUserAircraft& userAircraft, bool allowSplit);

  /* Get maximum altitude in all track points */
  float getMaxAltitude() const
  {
    return maximumAltitude;
  }

  /* Copies the coordinates from the structs to a list of linestrings.
   * More than one linestring might be returned if the trail is interrupted. */
  const QVector<atools::geo::LineString> getLineStrings() const;

  /* Track will be pruned if it contains more track entries than this value. Default is 20000. */
  void setMaxTrackEntries(int value)
  {
    maxTrackEntries = value;
  }

  /* Write and read the whole track to and from a binary stream */
  void saveToStream(QDataStream& out);

  bool readFromStream(QDataStream & in);

  /* Pull only needed methods of the base class into public space */
  using QList<at::AircraftTrailPos>::isEmpty;
  using QList<at::AircraftTrailPos>::at;
  using QList<at::AircraftTrailPos>::size;
  using QList<at::AircraftTrailPos>::constBegin;
  using QList<at::AircraftTrailPos>::constEnd;
  using QList<at::AircraftTrailPos>::begin;
  using QList<at::AircraftTrailPos>::end;
  using QList<at::AircraftTrailPos>::constFirst;
  using QList<at::AircraftTrailPos>::constLast;

private:
  friend QDataStream& at::operator>>(QDataStream& dataStream, at::AircraftTrailPos& trackPos);

  void calculateMaxAltitude();
  void updateMaxAltitude(const at::AircraftTrailPos& trackPos);

  /* Accurate positions for drawing */
  const QVector<QVector<atools::geo::PosD> > getPositionsD() const;

  /* Same size as getLineStrings() but returns the timestamps in milliseconds since Epoch UTC for each position */
  const QVector<QVector<qint64> > getTimestampsMs() const;

  /* Maximum number of track points. If exceeded entries will be removed from beginning of the list */
  int maxTrackEntries = 20000;

  float maximumAltitude = 0.f;

  atools::fs::sc::SimConnectUserAircraft *lastUserAircraft;

  /* Needed in RouteExportFormat stream operators to read different formats */
  static quint16 version;
};

#endif // LITTLENAVMAP_AIRCRAFTTRACK_H