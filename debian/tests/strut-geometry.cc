#include "EwmhStrut.hh"
#include <algorithm>
#include <cstdlib>
#include <iostream>

static int checks = 0;
static void check(const char *name, const unsigned long *v, int rw, int rh,
                  int x, int y, int w, int h, int l, int r, int t, int b) {
    int area[4];
    EwmhStrut::partial(v, rw, rh, x, y, w, h, area);
    const int expected[4] = {l, r, t, b};
    if (!std::equal(area, area + 4, expected)) {
        std::cerr << "FAIL " << name << ": " << area[0] << ',' << area[1]
                  << ',' << area[2] << ',' << area[3] << '\n';
        std::exit(1);
    }
    ++checks;
    std::cout << "PASS geometry: " << name << '\n';
}

int main() {
    unsigned long v[12] = {0,0,0,53,0,0,0,0,0,0,0,1279};
    check("bottom panel", v, 1280,800, 0,0,1280,800, 0,0,0,53);
    v[3] = 65;
    check("resized panel", v, 1280,800, 0,0,1280,800, 0,0,0,65);
    v[3] = 0;
    check("zero reservation", v, 1280,800, 0,0,1280,800, 0,0,0,0);
    v[3] = 230; v[10] = 800; v[11] = 1599;
    check("short right monitor", v, 1600,1200, 800,0,800,1000, 0,0,0,30);
    check("unaffected left monitor", v, 1600,1200, 0,0,800,1200, 0,0,0,0);
    v[10] = 0;
    check("spanning panel left", v, 1600,1200, 0,0,800,1200, 0,0,0,230);
    check("spanning panel right", v, 1600,1200, 800,0,800,1000, 0,0,0,30);
    v[3] = 30;
    check("strut below short monitor", v, 1600,1200, 800,0,800,1000, 0,0,0,0);
    v[11] = 799;
    check("inclusive last pixel", v, 1600,1200, 0,0,800,1200, 0,0,0,30);
    check("exclusive adjacent monitor", v, 1600,1200, 800,0,800,1200, 0,0,0,0);
    v[10] = 800;
    check("reversed interval", v, 1600,1200, 0,0,1600,1200, 0,0,0,0);
    v[10] = 1600; v[11] = 0xffffffffUL;
    check("out of screen interval", v, 1600,1200, 0,0,1600,1200, 0,0,0,0);
    std::fill(v, v+12, 0UL); v[2]=40; v[8]=0; v[9]=1599;
    check("top panel", v, 1600,1200, 0,0,800,1200, 0,0,40,0);
    check("top panel above lower monitor", v, 1600,1200, 800,200,800,1000, 0,0,0,0);
    std::fill(v, v+12, 0UL); v[0]=25; v[4]=0; v[5]=1199;
    check("left panel", v, 1600,1200, 0,0,800,1200, 25,0,0,0);
    check("left panel not on right monitor", v, 1600,1200, 800,0,800,1200, 0,0,0,0);
    std::fill(v, v+12, 0UL); v[1]=25; v[6]=0; v[7]=1199;
    check("right panel", v, 1600,1200, 800,0,800,1200, 0,25,0,0);
    std::fill(v, v+12, 0xffffffffUL);
    v[4]=v[6]=v[8]=v[10]=0;
    check("CARDINAL overflow leaves one pixel", v, 1280,800, 0,0,1280,800, 1279,0,799,0);
    check("empty head", v, 1280,800, 0,0,0,0, 0,0,0,0);
    std::cout << "Geometry tests: " << checks << " passed\n";
}
