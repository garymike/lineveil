/* Single source of truth for the version. The release workflow checks
   that the pushed tag (vMAJOR.MINOR.PATCH) matches these numbers. */
#define LV_VERSION_MAJOR 0
#define LV_VERSION_MINOR 1
#define LV_VERSION_PATCH 1

#define LV_STR2(x) #x
#define LV_STR(x) LV_STR2(x)
#define LV_VERSION_STR LV_STR(LV_VERSION_MAJOR) "." LV_STR(LV_VERSION_MINOR) "." LV_STR(LV_VERSION_PATCH)
