#include "catch.hpp"
#include "libslic3r.h"
#include "Point.hpp"
#include "Line.hpp"

namespace Slic3r {

// custom matcher to test point has expected coordinates
class IsPoint : public Catch::MatcherBase<Point> {
    coord_t x, y;
public:
    IsPoint( coord_t x, coord_t y ) : x( x ), y( y ) {}
    virtual bool match( Point const& p ) const override {
        return p.x == x && p.y == y;
    }
    virtual std::string describe() const {
        std::ostringstream ss;
        ss << " is (" << x << ", " << y << ")";
        return ss.str();
    }
};


TEST_CASE("Point") {
    Point point(10, 15);
    REQUIRE_THAT(point, IsPoint(10, 15));

    SECTION( "operators" ) {
        Point point2(10, 15);

        REQUIRE( point == point2);

        Point point3 = point + point2;
        REQUIRE_THAT(point3, IsPoint(20, 30));
    }

    SECTION( "translate" ) {
        point.translate(10, -5);
        REQUIRE_THAT(point, IsPoint(20, 10));
    }

    SECTION( "coincides_with") {
        Point point2 = point;
        REQUIRE( point.coincides_with(point2) );
        point2.translate(10, 20);
        REQUIRE_FALSE( point.coincides_with(point2) );
    }

    SECTION( "nearest_point" ) {
        Point point2(20, 30);
        Point nearest;
        Points points = {point2, Point(100, 200)};
        point.nearest_point(points, &nearest);
        REQUIRE( nearest.coincides_with(point2) );
    }

    SECTION( "point distance to horizontal line" ) {
        Line line(Point(0, 0), Point(100, 0));
        REQUIRE( Point(0, 0).distance_to(line) == 0 );
        REQUIRE( Point(100, 0).distance_to(line) == 0 );
        REQUIRE( Point(50, 0).distance_to(line) == 0 );
        REQUIRE( Point(150, 0).distance_to(line) == 50 );
        REQUIRE( Point(0, 50).distance_to(line) == 50 );
        REQUIRE( Point(50, 50).distance_to(line) == 50 );
        REQUIRE( Point(50, 50).perp_distance_to(line) == 50 );
        REQUIRE( Point(150, 50).perp_distance_to(line) == 50 );
    }

    SECTION( "point distance to falling line" ) {
        Line line(Point(50, 50), Point(125, -25));
        REQUIRE( Point(100, 0).distance_to(line) == 0 );
    }

    SECTION( "perp distance to line does not overflow" ) {
        Line line(Point(18335846, 18335845), Point(18335846, 1664160));
        REQUIRE( Point(1664161, 18335848).distance_to(line) == Approx(16671685) );
    }
    
    SECTION( "ccw() does not overflow" ) {
        Point p0(76975850, 89989996);
        Point p1(76989990, 109989991);
        Point p2(76989987, 89989994);
        REQUIRE( p0.ccw(p1, p2) < 0 );
    }

    SECTION( "project_onto_line" ) {
        Line line(Point(10, 10), Point(20, 10));

        REQUIRE_THAT( Point(15, 15).projection_onto(line), IsPoint(15, 10) );
        REQUIRE_THAT( Point(0 , 15).projection_onto(line), IsPoint(10, 10) );
        REQUIRE_THAT( Point(25, 15).projection_onto(line), IsPoint(20, 10) );
        REQUIRE_THAT( Point(10, 10).projection_onto(line), IsPoint(10, 10) );
        REQUIRE_THAT( Point(12, 10).projection_onto(line), IsPoint(12, 10) );
    }
}

}