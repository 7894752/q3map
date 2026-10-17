/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "qbsp.h"

char source[1024];
char tempsource[1024];
char name[1024];

float blockSize = 1024.0f;
vec_t microvolume = 1.0f;
qboolean glview;
qboolean nodetail;
qboolean fulldetail;
qboolean onlyents;
qboolean onlytextures;
qboolean nowater;
qboolean nofill;
qboolean noopt;
qboolean leaktest;
qboolean verboseentities;
qboolean noCurveBrushes;
qboolean fakemap;
qboolean nofog;
qboolean nosubdivide;
qboolean nomerge;
qboolean testExpand;
qboolean showseams;
char outbase[32];
int entity_num;

extern float brushSmoothAngle, curveSmoothAngle, terrainSmoothAngle;
extern qboolean colorLightmaps;

void ProcessWorldModel(void);
void ProcessSubModel(void);
void ProcessEntityPatches(entity_t *entity);
void OpenDebugFile(void);
void CloseDebugFile(void);
void PortalizeWorld(bspbrush_t *brushes, tree_t *tree, qboolean leaked);
void EmitDrawSurfaces(entity_t *entity, tree_t *tree);
void FilterCollisionSurfsIntoTree(entity_t *entity, tree_t *tree);
void Bspinfo(int count, char **fileNames);
int VisMain(int argc, char **argv);
void OnlyEnts(void);
void SetQ3MapErrorHandler(void);
void InitQ3MapFileSystem(void);
float SmoothAngleValue(const char *value, const char *name);
void ProcessCoronaLights(void);
void Swap_Init(void);

#define SMOOTH_ANGLE_RADIANS_PER_DEGREE ( (float) ( Q_PI / 180.0 ) )
#define SMOOTH_ANGLE_COSINE_EPSILON 0.001f

/*
SmoothAngleValue

Parses a smoothing angle option and returns its biased cosine threshold.
Endpoint options retain the historical threshold of one.
*/

void Q3MapFatalError(const char *format, va_list args);
void SetErrorHandler(void (*handler)(const char *, va_list));

char *FS_InitFilesystemFromArgs(char *basePath, char *gameDir, char *baseGame);

/*
============
ProcessWorldModel

============
*/
void ProcessWorldModel(void)
{
    bspface_t *faces;
    tree_t *tree;
    qboolean leaked;

    BeginModel();

    entities[0].firstDrawSurf = 0;
    ProcessEntityPatches(&entities[0]);
    PatchMapDrawSurfs(&entities[0]);

    faces = MakeStructuralBspFaceList(entities[0].brushes);
    tree = FaceBSP(faces);
    MakeTreePortals(tree);
    FilterStructuralBrushesIntoTree(&entities[0], tree);

    OpenDebugFile();

    if (FloodEntities(tree))
    {
        FillOutside(tree->headnode);
        ClipSidesIntoTree(&entities[0], tree);

        faces = MakeVisibleBspFaceList(entities[0].brushes);
        FreeTree(tree);
        tree = FaceBSP(faces);
        MakeTreePortals(tree);
        FilterStructuralBrushesIntoTree(&entities[0], tree);
        leaked = qfalse;
    }
    else
    {
        _printf("**********************\n");
        _printf("******* leaked *******\n");
        _printf("**********************\n");
        LeakFile(tree);

        if (leaktest)
        {
            _printf("--- MAP LEAKED, ABORTING LEAKTEST ---\n");
            exit(0);
        }

        leaked = qtrue;
        ClipSidesIntoTree(&entities[0], tree);
    }

    CloseDebugFile();

    PortalizeWorld(entities[0].brushes, tree, leaked);
    NumberClusters(tree);
    if (!leaked)
        WritePortalFile(tree);

    if (glview)
        WriteGLView(tree, source);

    FloodAreas(tree);
    FilterDetailBrushesIntoTree(&entities[0], tree);

    if (!nosubdivide)
        SubdivideDrawSurfs(&entities[0], tree);

    if (!nomerge)
        MergeSides(&entities[0], tree);

    FixTJunctions(&entities[0]);
    AllocateLightmaps(&entities[0]);
    EmitDrawSurfaces(&entities[0], tree);
    FilterCollisionSurfsIntoTree(&entities[0], tree);

    EndModel(tree->headnode);
    FreeTree(tree);
}

/*
============
ProcessSubModel

============
*/
void ProcessSubModel(void)
{
    entity_t *entity;
    node_t *node;
    bspbrush_t *brush, *newBrush;
    tree_t *tree;

    BeginModel();
    entity = &entities[entity_num];
    entity->firstDrawSurf = numMapDrawSurfs;
    ProcessEntityPatches(entity);
    PatchMapDrawSurfs(entity);

    node = AllocNode();
    node->planenum = PLANENUM_LEAF;
    for (brush = entity->brushes; brush; brush = brush->next) {
        newBrush = CopyBrush(brush);
        newBrush->next = node->brushlist;
        node->brushlist = newBrush;
    }

    tree = AllocTree();
    tree->headnode = node;
    ClipSidesIntoTree(entity, tree);
    if (!nosubdivide) SubdivideDrawSurfs(entity, tree);
    if (!nomerge) MergeSides(entity, tree);
    FixTJunctions(entity);
    AllocateLightmaps(entity);
    EmitDrawSurfaces(entity, tree);
    FilterCollisionSurfsIntoTree(entity, tree);
    EndModel(node);
    FreeTree(tree);
}

/*
============
ProcessModels
============
*/
void ProcessModels(void)
{
    qboolean oldVerbose;
    entity_t *entity;

    oldVerbose = verbose;
    BeginBSPFile();

    for (entity_num = 0; entity_num < num_entities; entity_num++)
    {
        entity = &entities[entity_num];

        if (!entity->brushes && !entity->patches)
            continue;

        qprintf("############### model %i ###############\n", nummodels);

        if (entity_num == 0)
            ProcessWorldModel();
        else
            ProcessSubModel();

        if (!verboseentities)
            verbose = qfalse;
    }

    verbose = oldVerbose;
}

/*
=============
Com_Printf

Both client and server can use this, and it will output
to the apropriate place.

A raw string should NEVER be passed as fmt, because of "%f" type crashers.
=============
*/
void Com_Printf(const char *format, ...)
{
    va_list argptr;
    char text[32768];

    va_start(argptr, format);
    vsprintf(text, format, argptr);
    va_end(argptr);

    _printf(text);
}

/*
=============
Com_Error

Both client and server can use this, and it will
do the apropriate things.
=============
*/
void Com_Error(int code, const char *format, ...)
{
    va_list argptr;
    char text[32768];

    (void)code;
    va_start(argptr, format);
    vsprintf(text, format, argptr);
    va_end(argptr);

    _printf(text);
}

/*
================
Com_DPrintf

A Com_Printf that only shows up if the "developer" cvar is set
================
*/
void Com_DPrintf(const char *format, ...)
{
}

/*
=============
InitQ3MapFileSystem
=============
*/
void InitQ3MapFileSystem(void)
{
	FS_InitFilesystemFromArgs(qdir, "", "");
}

/*
============
Bspinfo
============
*/
void Bspinfo(int count, char **fileNames)
{
    int i;
    char source[1024];
    int size;
    FILE *f;

    if (count < 1)
    {
        _printf("No files to dump info for.\n");
        return;
    }

    Swap_Init();

    for (i = 0; i < count; i++)
    {
        _printf("---------------------\n");
        strcpy(source, fileNames[i]);
        DefaultExtension(source, ".bsp");
        f = fopen(source, "rb");
        if (f)
        {
            size = Q_filelength(f);
            fclose(f);
        }
        else
        {
            size = 0;
        }
        _printf("%s: %i\n", source, size);

        LoadBSPFile(source);
        PrintBSPFileSizes(size);
        _printf("---------------------\n");
    }
}

/*
============
OnlyEnts
============
*/
void OnlyEnts(void)
{
    char out[1024];

    sprintf(out, "%s.bsp", source);
    LoadBSPFile(out);
    num_entities = 0;

    LoadMapFile(name);
    SetModelNumbers();
    SetLightStyles();
    ProcessCoronaLights();

    UnparseEntitiesWithOrigins();

    WriteBSPFile(out);
}

/*
============
OnlyTextures
============
*/
void OnlyTextures(void)
{
    char out[1024];
    int i;

    Error("-onlytextures isn't working now...");

    sprintf(out, "%s.bsp", source);

    LoadMapFile(name);
    LoadBSPFile(out);

    for (i = 0; i < numTriangleSurfaces; i++)
    {
    }

    WriteBSPFile(out);
}

/*
=============
Q3MapFatalError
=============
*/
void Q3MapFatalError(const char *format, va_list args)
{
	vprintf(format, args);
	_printf("\r\n");
	exit(1);
}

/*
=============
SetQ3MapErrorHandler
=============
*/
void SetQ3MapErrorHandler(void) {
    SetErrorHandler(Q3MapFatalError);
}

/*
=============
SmoothAngleValue
=============
*/
float SmoothAngleValue( const char *value, const char *name ) {
	float angle;
	float threshold;

	angle = atof( value );
	if ( angle <= 0.0f ) {
		printf( "%s = 0\n", name );
		return 1.0f;
	}
	if ( angle >= 180.0f ) {
		printf( "%s = 180\n", name );
		return 1.0f;
	}

	printf( "%s = %g\n", name, angle );
	threshold = (float) cos( angle * SMOOTH_ANGLE_RADIANS_PER_DEGREE )
		- SMOOTH_ANGLE_COSINE_EPSILON;
	if ( 0.0f > threshold ) {
		return 0.0f;
	}
	return threshold;
}

/*
============
main
============
*/
int main(int argc, char **argv)
{
    int i;
    double start, end;
    char baseDir[1024] = "Call of Duty";
    char path[1024];

    _printf("Q3Map v1.1 (c) 2002 Id Software Inc. / Infinity Ward\n");
    SetQ3MapErrorHandler();

    if (argc < 2)
        Error("usage: q3map [options] mapfile");

    if (!strcmp(argv[1], "-info"))
    {
        Bspinfo(argc - 2, argv + 2);
        return 0;
    }

    if (!strcmp(argv[1], "-light"))
    {
        Error("use FLARE for lighting\n");
        return 0;
    }

    if (!strcmp(argv[1], "-vlight"))
    {
        Error("use FLARE for lighting\n");
        return 0;
    }

    if (!strcmp(argv[1], "-vis"))
    {
        VisMain(argc - 1, argv + 1);
        return 0;
    }

    _printf("---- q3map ----\n");
    tempsource[0] = '\0';

    for (i = 1; i < argc; i++)
    {
        if (!stricmp(argv[i], "-tempname"))
        {
            strcpy(tempsource, argv[++i]);
        }
        else if (!stricmp(argv[i], "-threads"))
        {
            numthreads = atoi(argv[i + 1]);
            i++;
        }
        else if (!stricmp(argv[i], "-glview"))
        {
            glview = qtrue;
        }
        else if (!stricmp(argv[i], "-v"))
        {
            _printf("verbose = true\n");
            verbose = qtrue;
        }
        else if (!stricmp(argv[i], "-draw"))
        {
            _printf("drawflag = true\n");
            drawflag = qtrue;
        }
        else if (!stricmp(argv[i], "-nowater"))
        {
            _printf("nowater = true\n");
            nowater = qtrue;
        }
        else if (!stricmp(argv[i], "-noopt"))
        {
            _printf("noopt = true\n");
            noopt = qtrue;
        }
        else if (!stricmp(argv[i], "-nofill"))
        {
            _printf("nofill = true\n");
            nofill = qtrue;
        }
        else if (!stricmp(argv[i], "-nodetail"))
        {
            _printf("nodetail = true\n");
            nodetail = qtrue;
        }
        else if (!stricmp(argv[i], "-fulldetail"))
        {
            _printf("fulldetail = true\n");
            fulldetail = qtrue;
        }
        else if (!stricmp(argv[i], "-onlyents"))
        {
            _printf("onlyents = true\n");
            onlyents = qtrue;
        }
        else if (!stricmp(argv[i], "-onlytextures"))
        {
            _printf("onlytextures = true\n");
            onlytextures = qtrue;
        }
        else if (!stricmp(argv[i], "-micro"))
        {
            microvolume = atof(argv[i + 1]);
            _printf("microvolume = %f\n", microvolume);
            i++;
        }
        else if (!stricmp(argv[i], "-nofog"))
        {
            _printf("nofog = true\n");
            nofog = qtrue;
        }
        else if (!stricmp(argv[i], "-nosubdivide"))
        {
            _printf("nosubdivide = true\n");
            nosubdivide = qtrue;
        }
        else if (!stricmp(argv[i], "-leaktest"))
        {
            _printf("leaktest = true\n");
            leaktest = qtrue;
        }
        else if (!stricmp(argv[i], "-verboseentities"))
        {
            _printf("verboseentities = true\n");
            verboseentities = qtrue;
        }
        else if (!stricmp(argv[i], "-nocurves"))
        {
            noCurveBrushes = qtrue;
            _printf("no curve brushes\n");
        }
        else if (!stricmp(argv[i], "-expand"))
        {
            testExpand = qtrue;
            _printf("Writing expanded.map.\n");
        }
        else if (!stricmp(argv[i], "-showseams"))
        {
            showseams = qtrue;
            _printf("Showing seams on terrain.\n");
        }
        else if (!stricmp(argv[i], "-tmpout"))
        {
            strcpy(outbase, "/tmp");
        }
        else if (!stricmp(argv[i], "-fakemap"))
        {
            fakemap = qtrue;
            _printf("will generate fakemap.map\n");
        }
        else if (!stricmp(argv[i], "-blocksize"))
        {
            blockSize = atof(argv[i + 1]);
            if (blockSize > 0.0f && blockSize <= 64.0f)
                blockSize = 64.0f;
            else if (blockSize <= 0.0f || blockSize > 32768.0f)
                blockSize = 32768.0f;
            i++;
            _printf("blocksize is %g units\n", blockSize);
        }
        else if (!stricmp(argv[i], "-samplesize"))
        {
            samplesize = atoi(argv[i + 1]);
            if (samplesize < 1)
                samplesize = 1;
            i++;
            _printf("lightmap sample size is %dx%d units\n", samplesize, samplesize);
        }
        else if (!stricmp(argv[i], "-curveSmoothAngle"))
        {
            curveSmoothAngle = SmoothAngleValue(argv[i + 1], "curveSmoothAngle");
            i++;
        }
        else if (!stricmp(argv[i], "-terrainSmoothAngle"))
        {
            terrainSmoothAngle = SmoothAngleValue(argv[i + 1], "terrainSmoothAngle");
            i++;
        }
        else if (!stricmp(argv[i], "-brushSmoothAngle"))
        {
            brushSmoothAngle = SmoothAngleValue(argv[i + 1], "brushSmoothAngle");
            i++;
        }
        else if (!stricmp(argv[i], "-colorlmaps"))
        {
            colorLightmaps = qtrue;
        }
        else if (!stricmp(argv[i], "-basedir"))
        {
            strcpy(baseDir, argv[i + 1]);
            i++;
        }
        else if (argv[i][0] == '-')
        {
            Error("Unknown option \"%s\"", argv[i]);
        }
        else
        {
            break;
        }
    }

    if (i != argc - 1)
        Error("usage: q3map [options] mapfile");

    start = I_FloatTime();
    ThreadSetDefault();
    SetQdirFromPath(argv[i], baseDir);
    InitQ3MapFileSystem();

    strcpy(source, ExpandArg(argv[i]));
    StripExtension(source);

    sprintf(path, "%s.prt", source);
    remove(path);
    sprintf(path, "%s.lin", source);
    remove(path);

    strcpy(name, ExpandArg(argv[i]));
    if (strcmp(name + strlen(name) - 4, ".reg"))
    {
        sprintf(path, "%s.reg", source);
        remove(path);
        DefaultExtension(name, ".map");
    }

    LoadShaderInfo();

    if (onlyents)
    {
        OnlyEnts();
    }
    else if (onlytextures)
    {
        OnlyTextures();
    }
    else
    {
        if (strlen(tempsource) > 0)
            LoadMapFile(tempsource);
        else
            LoadMapFile(name);

        SetModelNumbers();
        SetLightStyles();
        ProcessCoronaLights();
        ProcessModels();
        EndBSPFile();

        end = I_FloatTime();
        _printf("%5.0f seconds elapsed\n", end - start);

        if (strlen(tempsource) > 0)
            remove(tempsource);
    }

    return 0;
}