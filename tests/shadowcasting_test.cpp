#include "catch/catch.hpp"

#include "line.h" // For rl_dist.
#include "map.h"
#include "test_helpers.h"

#include <chrono>
#include <random>
#include "stdio.h"

// Constants setting the ratio of set to unset tiles.
constexpr int NUMERATOR = 1;
constexpr int DENOMINATOR = 10;

// The width and height of the area being checked.
constexpr int DIMENSION = 121;

void oldCastLight( float (&output_cache)[MAPSIZE*SEEX][MAPSIZE*SEEY],
                   const float (&input_array)[MAPSIZE*SEEX][MAPSIZE*SEEY],
                   const int xx, const int xy, const int yx, const int yy,
                   const int offsetX, const int offsetY, const int offsetDistance,
                   const int row = 1, float start = 1.0f, const float end = 0.0f )
{

    float newStart = 0.0f;
    float radius = 60.0f - offsetDistance;
    if( start < end ) {
        return;
    }
    bool blocked = false;
    static const tripoint origin(0, 0, 0);
    tripoint delta(0, 0, 0);
    for( int distance = row; distance <= radius && !blocked; distance++ ) {
        delta.y = -distance;
        for( delta.x = -distance; delta.x <= 0; delta.x++ ) {
            int currentX = offsetX + delta.x * xx + delta.y * xy;
            int currentY = offsetY + delta.x * yx + delta.y * yy;
            float leftSlope = (delta.x - 0.5f) / (delta.y + 0.5f);
            float rightSlope = (delta.x + 0.5f) / (delta.y - 0.5f);

            if( start < rightSlope ) {
                continue;
            } else if( end > leftSlope ) {
                break;
            }

            //check if it's within the visible area and mark visible if so
            if( rl_dist(origin, delta) <= radius ) {
                /*
                  float bright = (float) (1 - (rStrat.radius(delta.x, delta.y) / radius));
                  lightMap[currentX][currentY] = bright;
                */
                output_cache[currentX][currentY] = LIGHT_TRANSPARENCY_CLEAR;
            }

            if( blocked ) {
                //previous cell was a blocking one
                if( input_array[currentX][currentY] == LIGHT_TRANSPARENCY_SOLID ) {
                    //hit a wall
                    newStart = rightSlope;
                    continue;
                } else {
                    blocked = false;
                    start = newStart;
                }
            } else {
                if( input_array[currentX][currentY] == LIGHT_TRANSPARENCY_SOLID &&
                    distance < radius ) {
                    //hit a wall within sight line
                    blocked = true;
                    oldCastLight(output_cache, input_array, xx, xy, yx, yy,
                              offsetX, offsetY, offsetDistance,  distance + 1, start, leftSlope );
                    newStart = rightSlope;
                }
            }
        }
    }
}

void shadowcasting_runoff(int iterations) {
    // Construct a rng that produces integers in a range selected to provide the probability
    // we want, i.e. if we want 1/4 tiles to be set, produce numbers in the range 0-3,
    // with 0 indicating the bit is set.
    const unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<unsigned int> distribution(0, DENOMINATOR);
    auto rng = std::bind ( distribution, generator );

    float seen_squares_control[MAPSIZE*SEEX][MAPSIZE*SEEY] = {0};
    float seen_squares_experiment[MAPSIZE*SEEX][MAPSIZE*SEEY] = {0};
    float transparency_cache[MAPSIZE*SEEX][MAPSIZE*SEEY] = {0};

    // Initialize the transparency value of each square to a random value.
    for( auto &inner : transparency_cache ) {
        for( float &square : inner ) {
            if( rng() < NUMERATOR ) {
                square = 1.0f;
            }
        }
    }

    map dummy;

    const int offsetX = 65;
    const int offsetY = 65;


    struct timespec start1;
    struct timespec end1;
    clock_gettime( CLOCK_REALTIME, &start1 );
    for( int i = 0; i < iterations; i++ ) {
        // First the control algorithm.
        oldCastLight( seen_squares_control, transparency_cache, 0, 1, 1, 0, offsetX, offsetY, 0 );
        oldCastLight( seen_squares_control, transparency_cache, 1, 0, 0, 1, offsetX, offsetY, 0 );

        oldCastLight( seen_squares_control, transparency_cache, 0, -1, 1, 0, offsetX, offsetY, 0 );
        oldCastLight( seen_squares_control, transparency_cache, -1, 0, 0, 1, offsetX, offsetY, 0 );

        oldCastLight( seen_squares_control, transparency_cache, 0, 1, -1, 0, offsetX, offsetY, 0 );
        oldCastLight( seen_squares_control, transparency_cache, 1, 0, 0, -1, offsetX, offsetY, 0 );

        oldCastLight( seen_squares_control, transparency_cache, 0, -1, -1, 0, offsetX, offsetY, 0 );
        oldCastLight( seen_squares_control, transparency_cache, -1, 0, 0, -1, offsetX, offsetY, 0 );
    }
    clock_gettime( CLOCK_REALTIME, &end1 );

    struct timespec start2;
    struct timespec end2;
    clock_gettime( CLOCK_REALTIME, &start2 );
    for( int i = 0; i < iterations; i++ ) {
        // Then the current algorithm.
        castLight<0, 1, 1, 0, sight_calc, sight_check>(
            seen_squares_experiment, transparency_cache, offsetX, offsetY, 0 );
        castLight<1, 0, 0, 1, sight_calc, sight_check>(
            seen_squares_experiment, transparency_cache, offsetX, offsetY, 0 );

        castLight<0, -1, 1, 0, sight_calc, sight_check>(
            seen_squares_experiment, transparency_cache, offsetX, offsetY, 0 );
        castLight<-1, 0, 0, 1, sight_calc, sight_check>(
            seen_squares_experiment, transparency_cache, offsetX, offsetY, 0 );

        castLight<0, 1, -1, 0, sight_calc, sight_check>(
            seen_squares_experiment, transparency_cache, offsetX, offsetY, 0 );
        castLight<1, 0, 0, -1, sight_calc, sight_check>(
            seen_squares_experiment, transparency_cache, offsetX, offsetY, 0 );

        castLight<0, -1, -1, 0, sight_calc, sight_check>(
            seen_squares_experiment, transparency_cache, offsetX, offsetY, 0 );
        castLight<-1, 0, 0, -1, sight_calc, sight_check>(
            seen_squares_experiment, transparency_cache, offsetX, offsetY, 0 );
    }
    clock_gettime( CLOCK_REALTIME, &end2 );

    struct timespec diff1;
    struct timespec diff2;
    timespec_subtract( &diff1, &end1, &start1 );
    timespec_subtract( &diff2, &end2, &start2 );

    if( iterations > 1 ) {
        // TODO: display this better, I doubt sec.nsec is an accurate rendering,
        // or at least reliably so.
        printf( "oldCastLight() executed %d times in %ld.%ld seconds.\n",
                iterations, diff1.tv_sec, diff1.tv_nsec );
        printf( "castLight() executed %d times in %ld.%ld seconds.\n",
                iterations, diff2.tv_sec, diff2.tv_nsec );
    }

    bool passed = true;
    for( int x = 0; passed && x < MAPSIZE*SEEX; ++x ) {
        for( int y = 0; y < MAPSIZE*SEEX; ++y ) {
            // Check that both agree on the outcome, but not necessarally the same values.
            if( (seen_squares_control[x][y] > LIGHT_TRANSPARENCY_SOLID) !=
                (seen_squares_experiment[x][y] > LIGHT_TRANSPARENCY_SOLID) ) {
                passed = false;
                break;
            }
        }
    }

    if( !passed ) {
        for( int x = 0; x < MAPSIZE*SEEX; ++x ) {
            for( int y = 0; y < MAPSIZE*SEEX; ++y ) {
                char output = ' ';
                if( transparency_cache[x][y] == 1.0f ) {
                    output = '#';
                }
                if( seen_squares_control[x][y] != seen_squares_experiment[x][y] ) {
                    if( seen_squares_control[x][y] ) {
                        output = 'X';
                    } else {
                        output = 'x';
                    }
                }
                printf("%c", output);
            }
            printf("\n");
        }
        for( int x = 0; x < MAPSIZE*SEEX; ++x ) {
            for( int y = 0; y < MAPSIZE*SEEX; ++y ) {
                char output = ' ';
                if( transparency_cache[x][y] == 1.0f ) {
                    output = '#';
                }
                if( !seen_squares_control[x][y] ) {
                    output = 'X';
                }
                printf("%c", output);
            }
            printf("    ");
            for( int y = 0; y < MAPSIZE*SEEX; ++y ) {
                char output = ' ';
                if( transparency_cache[x][y] == 1.0f ) {
                    output = '#';
                }
                if( !seen_squares_experiment[x][y] ) {
                    output = 'X';
                }
                printf("%c", output);
            }
            printf("\n");
        }
    }

    REQUIRE( passed );
}

TEST_CASE("Regression test against old shadowcasting implementation.") {
    shadowcasting_runoff(1);
}

TEST_CASE("Performance test old vs new shadowcasting algorithms.", "[.]") {
    shadowcasting_runoff(100000);
}
