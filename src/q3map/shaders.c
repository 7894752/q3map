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
#include "../universal/q_parse.h"
#include "../universal/com_math.h"

#define MAX_SURFACE_INFO 4096

shaderInfo_t defaultInfo;
shaderInfo_t shaderInfo[MAX_SURFACE_INFO];
int numShaderInfo;

typedef char shaderInfo_size_must_be_452[(sizeof(shaderInfo_t) == 452) ? 1 : -1];

int FS_ReadFile(const char *path, void **buffer);
void FS_FreeFile(void *buffer);
char **FS_ListFiles(const char *path, const char *extension, int *count);
void FS_FreeFileList(char **list);
char *Com_Parse(char **text);
void Com_BeginParseSession(const char *name);
void Com_EndParseSession(void);
void Com_ScriptWarning(const char *format, ...);
void Com_SkipRestOfLine(char **text);
void LoadDDSBuffer(byte *buffer, unsigned int length, byte **pic,
                   int *width, int *height, int *format, const char *name);
void LoadJPGBuff(byte *buffer, int length, byte **pic, int *width, int *height);

/*
=============
LoadShaderImageFile
=============
*/
qboolean LoadShaderImageFile(const char *filename, shaderInfo_t *si)
{
    byte *buffer = NULL;
    byte *pixels;
    int format;
    qboolean result;
    int length, nameLength;
    char name[1024];

    nameLength = strlen(filename);
    if (nameLength > 4 && filename[nameLength - 4] == '.') nameLength -= 4;
    if (nameLength + 4 >= sizeof(name)) {
        printf("filename '%s' is too long\n", filename);
        return qfalse;
    }
    memcpy(name, filename, nameLength);
    name[nameLength] = '.';
    name[nameLength + 4] = 0;
    name[nameLength + 1] = 'd';
    name[nameLength + 2] = 'd';
    name[nameLength + 3] = 's';
    length = FS_ReadFile(name, (void **)&buffer);
    if (length > 0) {
        LoadDDSBuffer(buffer, length, &pixels, &si->width, &si->height, &format, name);
        FS_FreeFile(buffer);
        free(pixels);
        return qtrue;
    }
    name[nameLength + 1] = 't';
    name[nameLength + 2] = 'g';
    name[nameLength + 3] = 'a';
    if (FS_ReadFile(name, (void **)&buffer) > 0) {
        LoadTGABuffer(buffer, &pixels, &si->width, &si->height, name);
        FS_FreeFile(buffer);
        free(pixels);
        return qtrue;
    }
    name[nameLength + 1] = 'j';
    name[nameLength + 2] = 'p';
    name[nameLength + 3] = 'g';
    length = FS_ReadFile(name, (void **)&buffer);
    if (length > 0) {
        LoadJPGBuff(buffer, length, &pixels, &si->width, &si->height);
        FS_FreeFile(buffer);
        free(pixels);
        result = qtrue;
    } else {
        result = qfalse;
    }
    return result;
}

/*
===============
LoadShaderImage
===============
*/
static void LoadShaderImage(shaderInfo_t *si)
{
    char filename[1024];
    if (si->lightimage[0]) {
        strcpy(filename, si->lightimage);
        DefaultExtension(filename, ".tga");
        if (LoadShaderImageFile(filename, si)) return;
    }
    if (si->editorimage[0]) {
        strcpy(filename, si->editorimage);
        DefaultExtension(filename, ".tga");
        if (LoadShaderImageFile(filename, si)) return;
    }
    strcpy(filename, si->shader);
    DefaultExtension(filename, ".tga");
    if (LoadShaderImageFile(filename, si)) return;
    _printf("WARNING: Couldn't find image for shader %s\n", si->shader);
    si->width = 64;
    si->height = 64;
}

/*
===============
AllocShaderInfo
===============
*/
static shaderInfo_t *AllocShaderInfo(void)
{
    shaderInfo_t *si;
    if (numShaderInfo == MAX_SURFACE_INFO) Error("MAX_SURFACE_INFO");
    si = &shaderInfo[numShaderInfo];
    numShaderInfo++;
    si->contents = CONTENTS_SOLID;
    si->surfaceFlags = 0;
    si->backsplashFraction = 0.05f;
    si->backsplashDistance = 24;
    si->clampTexture = qfalse;
    si->smoothingGroup = 0;
    si->lightmapSampleSize = 0;
    si->forceTraceLight = qfalse;
    si->forceVLight = qfalse;
    si->patchShadows = qfalse;
    si->vertexShadows = qfalse;
    si->noVertexShadows = qfalse;
    si->forceSunLight = qfalse;
    si->vertexScale = 1.0f;
    si->notjunc = qfalse;
    return si;
}

/*
===============
ShaderInfoForShader
===============
*/
shaderInfo_t *ShaderInfoForShader(const char *shaderName)
{
    int i;
    shaderInfo_t *si;
    char shader[128];
    char shaderType[128];
    const char *type, *base, *p;

    strcpy(shader, shaderName);
    StripExtension(shader);
    for (i = 0; i < numShaderInfo; i++) {
        si = &shaderInfo[i];
        if (!Q_stricmp(shader, si->shader)) {
            if (!si->width) {
                if (strrchr(shader, '@')) Error("'@' found in an explicitly specified shader name\n");
                LoadShaderImage(si);
            }
            return si;
        }
    }
    type = strrchr(shader, '@');
    if (type) {
        base = shader;
        if (*shader) {
            for (p = shader; *p; p++) if (*p == '\\' || *p == '/') base = p;
        }
        if (base > type) Error("image '%s' has a '/' or '\\' after an '@'\n", shader);
        else base++;
        strcpy(shaderType, "@world/");
        strncat(shaderType + 7, base, type - base);
        for (i = 0; i < numShaderInfo; i++) {
            si = &shaderInfo[i];
            if (!Q_stricmp(shaderType, si->shader)) {
                shaderInfo_t *newSi = AllocShaderInfo();
                *newSi = *si;
                newSi->lightimage[0] = 0;
                newSi->editorimage[0] = 0;
                strcpy(newSi->shader, shader);
                LoadShaderImage(newSi);
                return newSi;
            }
        }
        Error("shader type '%s' not found\n", shaderType + 1);
    }
    si = AllocShaderInfo();
    strcpy(si->shader, shader);
    LoadShaderImage(si);
    return si;
}

/*
=================
ParseShader

The current text pointer is at the explicit text definition of the
shader.  Parse it into the global shader variable.  Later functions
will optimize it.
=================
*/
static void ParseShader(const char *shaderName, char **text)
{
    const char *token;
    int i;
    shaderInfo_t *shader;
    shader = AllocShaderInfo();
    strcpy(shader->shader, shaderName);
    Com_MatchToken(text, "{", 0);
    while (1) {
        token = Com_Parse(text);
        if (!token[0]) break;
        if (!strcmp(token, "}")) break;
        if (!strcmp(token, "{")) {
            int depth;
            const char *stageToken;
            shader->hasPasses = qtrue;
            depth = 1;
            for (stageToken = Com_Parse(text); stageToken[0]; stageToken = Com_Parse(text)) {
                if (!strcmp(stageToken, "}")) {
                    if (!--depth) break;
                } else if (!strcmp(stageToken, "{")) {
                    depth++;
                } else if (!strcmp(stageToken, "map")) {
                    const char *mapToken;
                    for (mapToken = Com_ParseOnLine(text); mapToken[0]; mapToken = Com_ParseOnLine(text)) {
                        if (!strnicmp(mapToken, "clamp", 5)) {
                            char suffix = tolower(mapToken[5]);
                            if (!suffix || suffix == 'x' || suffix == 'y' || suffix == 's' || suffix == 't') shader->clampTexture = qtrue;
                        }
                    }
                }
            }
            continue;
        }
        if (!Q_stricmp(token, "surfaceparm")) {
            token = Com_ParseOnLine(text);
            for (i = 0; infoParms[i].name; i++) {
                if (!Q_stricmp(token, infoParms[i].name)) {
                    shader->surfaceFlags |= infoParms[i].surfaceFlags;
                    shader->contents |= infoParms[i].contents;
                    if (infoParms[i].clearSolid) shader->contents &= ~CONTENTS_SOLID;
                    break;
                }
            }
            if (!infoParms[i].name) {
                if (!Q_stricmp(token, "occluder")) shader->isOccluder = qtrue;
                else if (Q_strncasecmp(token, "qer", 3) && Q_stricmp(token, "noshadow")) _printf("Unknown surfaceparm: \"%s\"\n", token);
            }
            continue;
        }
        if (!Q_stricmp(token, "qer_editorimage")) {
            strcpy(shader->editorimage, Com_ParseOnLine(text));
            DefaultExtension(shader->editorimage, ".tga");
            continue;
        }
        if (!Q_stricmp(token, "q3map_smoothinggroup")) {
            Com_ParseOnLine(text);
            continue;
        }
        if (!Q_stricmp(token, "q3map_lightimage")) {
            strcpy(shader->lightimage, Com_ParseOnLine(text));
            DefaultExtension(shader->lightimage, ".tga");
            continue;
        }
        if (!Q_stricmp(token, "q3map_surfacelight")) {
            shader->value = atoi(Com_ParseOnLine(text));
            continue;
        }
        if (!Q_stricmp(token, "q3map_lightsubdivide")) {
            shader->lightSubdivide = atoi(Com_ParseOnLine(text));
            continue;
        }
        if (!Q_stricmp(token, "q3map_lightmapsamplesize")) {
            shader->lightmapSampleSize = atoi(Com_ParseOnLine(text));
            continue;
        }
        if (!Q_stricmp(token, "q3map_tracelight")) {
            shader->forceTraceLight = qtrue;
            continue;
        }
        if (!Q_stricmp(token, "q3map_vlight")) {
            shader->forceVLight = qtrue;
            continue;
        }
        if (!Q_stricmp(token, "q3map_patchshadows")) {
            shader->patchShadows = qtrue;
            continue;
        }
        if (!Q_stricmp(token, "q3map_vertexshadows")) {
            shader->vertexShadows = qtrue;
            continue;
        }
        if (!Q_stricmp(token, "q3map_novertexshadows")) {
            shader->noVertexShadows = qtrue;
            continue;
        }
        if (!Q_stricmp(token, "q3map_forcesunlight")) {
            shader->forceSunLight = qtrue;
            continue;
        }
        if (!Q_stricmp(token, "q3map_vertexscale")) {
            shader->vertexScale = atof(Com_ParseOnLine(text));
            continue;
        }
        if (!Q_stricmp(token, "q3map_notjunc")) {
            shader->notjunc = qtrue;
            continue;
        }
        if (!Q_stricmp(token, "q3map_globaltexture")) {
            shader->globalTexture = qtrue;
            continue;
        }
        if (!Q_stricmp(token, "q3map_backsplash")) {
            shader->backsplashFraction = atof(Com_ParseOnLine(text));
            shader->backsplashFraction *= 0.01f;
            shader->backsplashDistance = atof(Com_ParseOnLine(text));
            continue;
        }
        if (!Q_stricmp(token, "q3map_backshader")) {
            strcpy(shader->backShader, Com_ParseOnLine(text));
            continue;
        }
        if (!Q_stricmp(token, "q3map_flare")) {
            strcpy(shader->flareShader, Com_ParseOnLine(text));
            continue;
        }
        if (!Q_stricmp(token, "light")) {
            Com_ParseOnLine(text);
            strcpy(shader->flareShader, "flareshader");
            continue;
        }
        if (!Q_stricmp(token, "q3map_sun")) {
            float a, b;
            token = Com_ParseOnLine(text);
            shader->sunLight[0] = atof(token);
            token = Com_ParseOnLine(text);
            shader->sunLight[1] = atof(token);
            token = Com_ParseOnLine(text);
            shader->sunLight[2] = atof(token);
            VectorNormalize(shader->sunLight);
            token = Com_ParseOnLine(text);
            a = atof(token);
            VectorScale(shader->sunLight, a, shader->sunLight);
            token = Com_ParseOnLine(text);
            a = atof(token);
            a *= 0.017453292f;
            token = Com_ParseOnLine(text);
            b = atof(token);
            b *= 0.017453292f;
            shader->sunDirection[0] = cos(a) * cos(b);
            shader->sunDirection[1] = sin(a) * cos(b);
            shader->sunDirection[2] = sin(b);
            shader->surfaceFlags |= SURF_SKY;
            continue;
        }
        if (!Q_stricmp(token, "tesssize")) {
            shader->subdivisions = atof(Com_ParseOnLine(text));
            continue;
        }
        if (!Q_stricmp(token, "cull")) {
            token = Com_ParseOnLine(text);
            if (!Q_stricmp(token, "none")) shader->twoSided = qtrue;
            continue;
        }
        if (!Q_stricmp(token, "deformVertexes")) {
            token = Com_ParseOnLine(text);
            if (!Q_strncasecmp(token, "autosprite", 10)) {
                shader->autosprite = qtrue;
                shader->contents = CONTENTS_DETAIL;
            } else if (!Q_stricmp(token, "flap")) shader->flap = qtrue;
            continue;
        }
        Com_SkipRestOfLine(text);
    }
}

/*
===============
ParseShaderFile
===============
*/
static void ParseShaderFile(const char *filename)
{
    char *buffer, *text;
    char *token;
    if (FS_ReadFile(filename, (void **)&buffer) < 0) {
        _printf("WARNING: couldn't load shader '%s'\n", filename);
        return;
    }
    Com_BeginParseSession(filename);
    text = buffer;
    token = Com_Parse(&text);
    while (token[0]) {
        if (!isalnum(token[0]) && token[0] != '_' && token[0] != '/' && token[0] != '.') {
            Com_ScriptWarning("unexpected shader name '%s', skipping rest of file\n", token);
            break;
        }
        ParseShader(token, &text);
        token = Com_Parse(&text);
    }
    Com_EndParseSession();
    FS_FreeFile(buffer);
}

/*
=============
ParseShaderTypeFile
=============
*/
void ParseShaderTypeFile(const char *shaderName, const char *filename)
{
    char *buffer, *text;
    if (FS_ReadFile(filename, (void **)&buffer) < 0) {
        _printf("WARNING: couldn't load shader type '%s'\n", filename);
        return;
    }
    Com_BeginParseSession(filename);
    text = buffer;
    ParseShader(shaderName, &text);
    Com_EndParseSession();
    FS_FreeFile(buffer);
}

/*
===============
LoadShaderInfo
===============
*/
void LoadShaderInfo(void)
{
    char **files;
    int i, count;
    char shaderType[1024], filename[1024];
    files = FS_ListFiles("scripts", ".shader", &count);
    for (i = 0; i < count; i++) {
        sprintf(filename, "scripts/%s", files[i]);
        ParseShaderFile(filename);
    }
    FS_FreeFileList(files);
    files = FS_ListFiles("shadertypes/world", ".stype", &count);
    for (i = 0; i < count; i++) {
        strcpy(shaderType, "@world/");
        strcpy(shaderType + 7, files[i]);
        shaderType[strlen(shaderType) - 6] = 0;
        sprintf(filename, "shadertypes/world/%s", files[i]);
        ParseShaderTypeFile(shaderType, filename);
    }
    qprintf("%5i shaderInfo\n", numShaderInfo);
}


