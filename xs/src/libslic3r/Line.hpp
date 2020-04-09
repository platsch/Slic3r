#ifndef slic3r_Line_hpp_
#define slic3r_Line_hpp_

#include "libslic3r.h"
#include "Point.hpp"

namespace Slic3r {

class Line;
class Line3;
class Linef3;
class Polyline;
class ThickLine;
typedef std::vector<Line> Lines;
typedef std::vector<Line3> Line3s;
typedef std::vector<ThickLine> ThickLines;

class Line
{
    public:
    Point a;
    Point b;
    Line() {};
    explicit Line(Point _a, Point _b): a(_a), b(_b) {};
    std::string wkt() const;
    operator Lines() const;
    operator Polyline() const;
    void scale(double factor);
    void translate(double x, double y);
    void rotate(double angle, const Point &center);
    void reverse();
    double length() const;
    Point midpoint() const;
    void point_at(double distance, Point* point) const;
    Point point_at(double distance) const;
    bool intersection_infinite(const Line &other, Point* point) const;
    bool coincides_with(const Line &line) const;
    double distance_to(const Point &point) const;
    bool parallel_to(double angle) const;
    bool parallel_to(const Line &line) const;
    /// Returns true if both lines are overlapping but not identical.
    bool overlap_with(const Line &other, Line* line) const;
    /// Check if this line fully contains the other line. No common endpoints.
    bool contains(const Line &other) const;
    /// Check if point is on this line
    bool contains(const Point &point) const;
    double atan2_() const;
    double orientation() const;
    double direction() const;
    Vector vector() const;
    Vector normal() const;
    void extend_end(double distance);
    void extend_start(double distance);
    bool intersection(const Line& line, Point* intersection) const;
    double ccw(const Point& point) const;
};

class ThickLine : public Line
{
    public:
    coordf_t a_width, b_width;
    
    ThickLine() : a_width(0), b_width(0) {};
    ThickLine(Point _a, Point _b) : Line(_a, _b), a_width(0), b_width(0) {};
};

std::ostream& operator<<(std::ostream &stm, const Line3 &line3);

class Line3 : public Line
{
    public:
    Point3 a;
    Point3 b;
    Line3() {};
    explicit Line3(Point3 _a, Point3 _b): a(_a), b(_b) {};
    explicit Line3(Line _l, coord_t _z): a(Point3(_l.a.x, _l.a.y, _z)), b(Point3(_l.b.x, _l.b.y, _z)) {};
    explicit Line3(Point _a, Point _b, coord_t _z): a(Point3(_a.x, _a.y, _z)), b(Point3(_b.x, _b.y, _z)) {};
    void translate(double x, double y, double z);
    double length() const;
    Point3 intersect_plane(coord_t z) const;
};

class Linef
{
    public:
    Pointf a;
    Pointf b;
    Linef() {};
    explicit Linef(Pointf _a, Pointf _b): a(_a), b(_b) {};
};

class Linef3
{
    public:
    Pointf3 a;
    Pointf3 b;
    Linef3() {};
    explicit Linef3(Pointf3 _a, Pointf3 _b): a(_a), b(_b) {};
    Pointf3 intersect_plane(double z) const;
    void scale(double factor);
};

}

// start Boost
#include <boost/polygon/polygon.hpp>
namespace boost { namespace polygon {
    template <>
    struct geometry_concept<Line> { typedef segment_concept type; };

    template <>
    struct segment_traits<Line> {
        typedef coord_t coordinate_type;
        typedef Point point_type;
    
        static inline point_type get(const Line& line, direction_1d dir) {
            return dir.to_int() ? line.b : line.a;
        }
    };
} }
// end Boost

#endif
