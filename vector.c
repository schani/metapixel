/* -*- c -*- */

/*
 * vector.c
 *
 * metapixel
 *
 * Copyright (C) 1997-1999 Mark Probst
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <math.h>

#include "vector.h"

Vector2D
MakeVector2D (double x, double y)
{
    Vector2D vec = { x, y };

    return vec;
}

Vector2D*
InitVector2D (Vector2D *vec, double x, double y)
{
    vec->x = x;
    vec->y = y;

    return vec;
}

Vector2D*
CopyVector2D (Vector2D *dest, const Vector2D *src)
{
    dest->x = src->x;
    dest->y = src->y;

    return dest;
}

double
Abs2D (const Vector2D *v)
{
    return sqrt(v->x * v->x + v->y * v->y);
}

Vector2D*
AddVectors2D (Vector2D *dest, const Vector2D *vec1, const Vector2D *vec2)
{
    dest->x = vec1->x + vec2->x;
    dest->y = vec1->y + vec2->y;

    return dest;
}

Vector2D*
SubVectors2D (Vector2D *dest, const Vector2D *vec1, const Vector2D *vec2)
{
    dest->x = vec1->x - vec2->x;
    dest->y = vec1->y - vec2->y;

    return dest;
}

Vector2D*
MultScalar2D (Vector2D *dest, double d, const Vector2D *vec)
{
    dest->x = d * vec->x;
    dest->y = d * vec->y;

    return dest;
}

Vector2D*
MultVectors2D (Vector2D *dest, const Vector2D *vec1, const Vector2D *vec2)
{
    dest->x = vec1->x * vec2->x;
    dest->y = vec1->y * vec2->y;

    return dest;
}

Vector2D*
Unity2D (Vector2D *dest, Vector2D *vec)
{
    return MultScalar2D(dest, 1 / Abs2D(vec), vec);
}

Vector2D*
Rectangular2DToPolar (Vector2D *dest, const Vector2D *src)
{
    dest->y = sqrt(src->x * src->x + src->y * src->y);
    if (src->y == 0.0)
    {
	if (src->x > 0)
	    dest->x = 0.0;
	else
	    dest->x = M_PI;
    }
    else
    {
	dest->x = atan(src->y / src->x);
	if (src->x < 0.0)
	    dest->x = M_PI + dest->x;
    }

    dest->x = fmod(dest->x, 2 * M_PI);

    return dest;
}

Vector2D*
Polar2DToRectangular (Vector2D *dest, const Vector2D *src)
{
    dest->x = cos(src->x) * src->y;
    dest->y = sin(src->x) * src->y;

    return dest;
}

Vector3D
MakeVector3D (double x, double y, double z)
{
    Vector3D vec = { x, y, z };

    return vec;
}

Vector3D*
InitVector3D (Vector3D *vec, double x, double y, double z)
{
    vec->x = x;
    vec->y = y;
    vec->z = z;

    return vec;
}

Vector3D*
CopyVector3D (Vector3D *dest, const Vector3D *src)
{
    dest->x = src->x;
    dest->y = src->y;
    dest->z = src->z;

    return dest;
}

double
Abs3D (const Vector3D *v)
{
    return sqrt(v->x * v->x + v->y * v->y + v->z * v->z);
}

Vector3D*
AddVectors3D (Vector3D *vec, const Vector3D *vec1, const Vector3D *vec2)
{
    vec->x = vec1->x + vec2->x;
    vec->y = vec1->y + vec2->y;
    vec->z = vec1->z + vec2->z;

    return vec;
}

Vector3D*
SubVectors3D (Vector3D *vec, const Vector3D *vec1, const Vector3D *vec2)
{
    vec->x = vec1->x - vec2->x;
    vec->y = vec1->y - vec2->y;
    vec->z = vec1->z - vec2->z;

    return vec;
}

Vector3D*
MultScalar3D (Vector3D *dest, double d, const Vector3D *vec)
{
    dest->x = d * vec->x;
    dest->y = d * vec->y;
    dest->z = d * vec->z;

    return dest;
}

Vector3D*
MultVectors3D (Vector3D *vec, const Vector3D *vec1, const Vector3D *vec2)
{
    vec->x = vec1->x * vec2->x;
    vec->y = vec1->y * vec2->y;
    vec->z = vec1->z * vec2->z;

    return vec;
}

double
DotProduct3D (const Vector3D *vec1, const Vector3D *vec2)
{
    return vec1->x * vec2->x + vec1->y * vec2->y + vec1->z * vec2->z;
}

Vector3D*
CrossProduct3D (Vector3D *vec, const Vector3D *vec1, const Vector3D *vec2)
{
    vec->x = vec1->y * vec2->z - vec2->y * vec1->z;
    vec->y = vec1->z * vec2->x - vec2->z * vec1->x;
    vec->z = vec1->x * vec2->y - vec2->x * vec1->y;

    return vec;
}

Vector3D*
Unity3D (Vector3D *dest, const Vector3D *vec)
{
    return MultScalar3D(dest, 1 / Abs3D(vec), vec);
}

Matrix3D*
InitMatrix3D (Matrix3D *mat)
{
    int x,
	y;

    for (x = 0; x < 3; ++x)
	for (y = 0; y < 3; ++y)
	    (*mat)[x][y] = 0.0;

    return mat;
}

Matrix3D*
CopyMatrix3D (Matrix3D *dest, const Matrix3D *src)
{
    int x,
	y;

    for (x = 0; x < 3; ++x)
	for (y = 0; y < 3; ++y)
	    (*dest)[x][y] = (*src)[x][y];

    return dest;
}

Vector3D*
MultMatrixVector3D (Vector3D *dest, const Matrix3D *mat, const Vector3D *vec)
{
    dest->x = (*mat)[0][0] * vec->x + (*mat)[0][1] * vec->y + (*mat)[0][2] * vec->z;
    dest->y = (*mat)[1][0] * vec->x + (*mat)[1][1] * vec->y + (*mat)[1][2] * vec->z;
    dest->z = (*mat)[2][0] * vec->x + (*mat)[2][1] * vec->y + (*mat)[2][2] * vec->z;

    return dest;
}

Matrix4D*
InitMatrix4D (Matrix4D *mat)
{
    int x,
	y;

    for (x = 0; x < 4; ++x)
	for (y = 0; y < 4; ++y)
	    (*mat)[x][y] = 0.0;

    return mat;
}

Matrix4D*
CopyMatrix4D (Matrix4D *dest, const Matrix4D *src)
{
    int x,
	y;

    for (x = 0; x < 4; ++x)
	for (y = 0; y < 4; ++y)
	    (*dest)[x][y] = (*src)[x][y];

    return dest;
}

Matrix4D*
MultMatrix4D (Matrix4D *dest, const Matrix4D *mat1, const Matrix4D *mat2)
{
    int x,
	y,
	i;

    for (x = 0; x < 4; ++x)
	for (y = 0; y < 4; ++y)
	{
	    (*dest)[x][y] = 0.0;
	    for (i = 0; i < 4; ++i)
		(*dest)[x][y] += (*mat1)[x][i] * (*mat2)[i][y];
	}

    return dest;
}

Vector3D*
ApplyTransformation (Vector3D *dest, const Matrix4D *mat, const Vector3D *vec)
{
    dest->x = (*mat)[0][0] * vec->x + (*mat)[0][1] * vec->y +
	(*mat)[0][2] * vec->z + (*mat)[0][3];
    dest->y = (*mat)[1][0] * vec->x + (*mat)[1][1] * vec->y +
	(*mat)[1][2] * vec->z + (*mat)[1][3];
    dest->z = (*mat)[2][0] * vec->x + (*mat)[2][1] * vec->y +
	(*mat)[2][2] * vec->z + (*mat)[2][3];
    
    return dest;
}

Matrix4D*
InitTranslationMatrix (Matrix4D *mat, double x, double y, double z)
{
    int i;

    InitMatrix4D(mat);

    for (i = 0; i < 4; ++i)
	(*mat)[i][i] = 1.0;

    (*mat)[0][3] = x;
    (*mat)[1][3] = y;
    (*mat)[2][3] = z;

    return mat;
}

Matrix4D*
InitXRotationMatrix (Matrix4D *mat, double theta)
{
    double sine = sin(theta),
	cosine = cos(theta);

    InitMatrix4D(mat);

    (*mat)[0][0] = 1.0;
    (*mat)[3][3] = 1.0;
    (*mat)[1][1] = cosine;
    (*mat)[1][2] = -sine;
    (*mat)[2][1] = sine;
    (*mat)[2][2] = cosine;

    return mat;
}

Matrix4D*
InitYRotationMatrix (Matrix4D *mat, double theta)
{
    double sine = sin(theta),
	cosine = cos(theta);

    InitMatrix4D(mat);

    (*mat)[1][1] = 1.0;
    (*mat)[3][3] = 1.0;
    (*mat)[0][0] = cosine;
    (*mat)[0][2] = sine;
    (*mat)[2][0] = -sine;
    (*mat)[2][2] = cosine;

    return mat;
}

Matrix4D*
InitZRotationMatrix (Matrix4D *mat, double theta)
{
    double sine = sin(theta),
	cosine = cos(theta);

    InitMatrix4D(mat);

    (*mat)[2][2] = 1.0;
    (*mat)[3][3] = 1.0;
    (*mat)[0][0] = cosine;
    (*mat)[0][1] = -sine;
    (*mat)[1][0] = sine;
    (*mat)[1][1] = cosine;

    return mat;
}

Matrix4x3*
InitMatrix4x3 (Matrix4x3 *mat)
{
    int i,
	j;

    for (i = 0; i < 4; ++i)
	for (j = 0; j < 3; ++j)
	    (*mat)[i][j] = 0.0;

    return mat;
}

Matrix4x3*
MakeMatrix4x3 (Matrix4x3 *dest,
	       const Vector3D *vec1, const Vector3D *vec2,
	       const Vector3D *vec3, const Vector3D *vec4)
{
    (*dest)[0][0] = vec1->x;
    (*dest)[0][1] = vec1->y;
    (*dest)[0][2] = vec1->z;

    (*dest)[1][0] = vec2->x;
    (*dest)[1][1] = vec2->y;
    (*dest)[1][2] = vec2->z;

    (*dest)[2][0] = vec3->x;
    (*dest)[2][1] = vec3->y;
    (*dest)[2][2] = vec3->z;

    (*dest)[3][0] = vec4->x;
    (*dest)[3][1] = vec4->y;
    (*dest)[3][2] = vec4->z;

    return dest;
}

Matrix4x3*
CopyMatrix4x3 (Matrix4x3 *dest, const Matrix4x3 *src)
{
    int i,
	j;

    for (i = 0; i < 4; ++i)
	for (j = 0; j < 3; ++j)
	    (*dest)[i][j] = (*src)[i][j];

    return dest;
}

Matrix4x3*
MultMatrix4x3 (Matrix4x3 *dest, const Matrix4D *mat1, const Matrix4x3 *mat2)
{
    int i,
	j,
	k;

    for (i = 0; i < 4; ++i)
	for (j = 0; j < 3; ++j)
	{
	    (*dest)[i][j] = 0.0;
	    for (k = 0; k < 4; ++k)
		(*dest)[i][j] += (*mat1)[i][k] * (*mat2)[k][j];
	}

    return dest;
}

Vector3D*
MultVector4DMatrix4x3 (Vector3D *dest,
		       const Vector4D *vec, const Matrix4x3 *mat)
{
    dest->x = (*vec)[0] * (*mat)[0][0] + (*vec)[1] * (*mat)[1][0] +
	(*vec)[2] * (*mat)[2][0] + (*vec)[3] * (*mat)[3][0];
    dest->y = (*vec)[0] * (*mat)[0][1] + (*vec)[1] * (*mat)[1][1] +
	(*vec)[2] * (*mat)[2][1] + (*vec)[3] * (*mat)[3][1];
    dest->z = (*vec)[0] * (*mat)[0][2] + (*vec)[1] * (*mat)[1][2] +
	(*vec)[2] * (*mat)[2][2] + (*vec)[3] * (*mat)[3][2];

    return dest;
}
