/*
===========================================================================
q3map (Call of Duty, 2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

/*
 * CoD IBSP59 light records as stored on disk.
 * Disk float vectors do not depend on the DOUBLEVEC_T option.
 */
#ifndef BSP_LIGHT_DISK_H
#define BSP_LIGHT_DISK_H

typedef enum {
    BSP_LIGHT_SUN = 1,
    BSP_LIGHT_POINT,
    BSP_LIGHT_LINEAR,
    BSP_LIGHT_ADJUSTED_POINT,
    BSP_LIGHT_SPOT,
    BSP_LIGHT_AREA,
    BSP_LIGHT_ADJUSTED_SPOT
} bspLightType_t;

typedef struct {
    float axis[2][3];
    float width;
    float height;
} bspAreaLight_t;

typedef struct {
    float inverseRange;
} bspLinearLight_t;

typedef struct {
    float quadratic;
    float constant;
} bspAdjustedPointLight_t;

typedef struct {
    float coneCosine;
    int exponent;
} bspSpotLight_t;

typedef struct {
    float quadratic;
    float constant;
    float coneCosine;
    int exponent;
} bspAdjustedSpotLight_t;

typedef struct {
    int type;
    float color[3];
    float origin[3];
    float direction[3];
    union {
        bspAreaLight_t area;
        bspLinearLight_t linear;
        bspAdjustedPointLight_t adjustedPoint;
        bspSpotLight_t spot;
        bspAdjustedSpotLight_t adjustedSpot;
    } data;
} bspLight_t;

#endif
