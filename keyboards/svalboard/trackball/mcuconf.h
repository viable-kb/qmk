#pragma once

#include_next <mcuconf.h>

// Enable SPI1 for PMW3360/3389 trackball (FFC connector)
#undef RP_SPI_USE_SPI1
#define RP_SPI_USE_SPI1 TRUE
