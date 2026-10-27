#include "World/Level/LevelGen/Noise/NoiseMath.h"

double NoiseMath::grad(int32_t hash, double x, double y, double z) {
    switch (hash & 0xF) {
        case 0x0:
            return x + y;
        case 0x1:
            return -x + y;
        case 0x2:
            return x - y;
        case 0x3:
            return -x - y;
        case 0x4:
            return x + z;
        case 0x5:
            return -x + z;
        case 0x6:
            return x - z;
        case 0x7:
            return -x - z;
        case 0x8:
            return y + z;
        case 0x9:
        case 0xD:
            return -y + z;
        case 0xA:
            return y - z;
        case 0xB:
        case 0xF:
            return -y - z;
        case 0xC:
            return y + x;
        case 0xE:
            return y - x;
        default:
            return 0.0;
    }
}

int64_t NoiseMath::lfloor(double d) {
    int64_t l = static_cast<int64_t>(d);
    return d < static_cast<double>(l) ? l - 1LL : l;
}

int32_t NoiseMath::floor(double d) {
    int32_t i = static_cast<int32_t>(d);
    return d < static_cast<double>(i) ? i - 1 : i;
}

double NoiseMath::lerp(double delta, double start, double end) {
    return start + delta * (end - start);
}

double NoiseMath::lerp2(double deltaX, double deltaY, double val00, double val10, double val01, double val11) {
    return lerp(deltaY, lerp(deltaX, val00, val10), lerp(deltaX, val01, val11));
}

double NoiseMath::lerp3(double deltaX, double deltaY, double deltaZ, double val000, double val100, double val010, double val110, double val001, double val101, double val011, double val111) {
    return lerp(deltaZ, lerp2(deltaX, deltaY, val000, val100, val010, val110), lerp2(deltaX, deltaY, val001, val101, val011, val111));
}

double NoiseMath::smoothStep(double d) {
    return d * d * d * (d * (d * 6.0 - 15.0) + 10.0);
}

double NoiseMath::maintainPrecision(double d) {
    return d - static_cast<double>(NoiseMath::lfloor(d / 3.3554432E7 + 0.5)) * 3.3554432E7;
}
