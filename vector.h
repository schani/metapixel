/* -*- c -*- */

/*
 * vector.h
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

typedef struct _Vector2D
{
    double x;
    double y;
} Vector2D;

typedef struct _Vector3D
{
    double x;
    double y;
    double z;
} Vector3D;

typedef double Vector4D[4];

typedef double Matrix3D[3][3];
typedef double Matrix4D[4][4];
typedef double Matrix4x3[4][3];

Vector2D MakeVector2D (double x, double y);
Vector2D* InitVector2D (Vector2D *vec, double x, double y);
Vector2D* CopyVector2D (Vector2D *dest, const Vector2D *src);
double Abs2D (const Vector2D *vec);
Vector2D* AddVectors2D (Vector2D *dest, const Vector2D *vec1,
			const Vector2D *vec2);
Vector2D* SubVectors2D (Vector2D *dest, const Vector2D *vec1,
			const Vector2D *vec2);
Vector2D* MultScalar2D (Vector2D *dest, double d, const Vector2D *vec);
Vector2D* MultVectors2D (Vector2D *vec, const Vector2D *vec1,
			 const Vector2D *vec2);
Vector2D* Unity2D (Vector2D *dest, Vector2D *vec);
Vector2D* Rectangular2DToPolar (Vector2D *dest, const Vector2D *src);
Vector2D* Polar2DToRectangular (Vector2D *dest, const Vector2D *src);

Vector3D MakeVector3D (double x, double y, double z);
Vector3D* InitVector3D (Vector3D *vec, double x, double y, double z);
Vector3D* CopyVector3D (Vector3D *dest, const Vector3D *src);
double Abs3D (const Vector3D *vec);
Vector3D* AddVectors3D (Vector3D *dest, const Vector3D *vec1,
			const Vector3D *vec2);
Vector3D* SubVectors3D (Vector3D *dest, const Vector3D *vec1,
			const Vector3D *vec2);
Vector3D* MultScalar3D (Vector3D *dest, double d, const Vector3D *vec);
Vector3D* MultVectors3D (Vector3D *vec, const Vector3D *vec1,
			 const Vector3D *vec2);
double DotProduct3D (const Vector3D *vec1, const Vector3D *vec2);
Vector3D* CrossProduct3D (Vector3D *vec, const Vector3D *vec1,
			  const Vector3D *vec2);
Vector3D* Unity3D (Vector3D *dest, const Vector3D *vec);

Matrix3D* InitMatrix3D (Matrix3D *mat);
Matrix3D* CopyMatrix3D (Matrix3D *dest, const Matrix3D *src);
Vector3D* MultMatrixVector3D (Vector3D *dest, const Matrix3D *mat, const Vector3D *vec);

Matrix4D* InitMatrix4D (Matrix4D *mat);
Matrix4D* CopyMatrix4D (Matrix4D *dest, const Matrix4D *src);
Matrix4D* MultMatrix4D (Matrix4D *dest,
			const Matrix4D *mat1, const Matrix4D *mat2);
Vector3D* ApplyTransformation (Vector3D *dest,
			       const Matrix4D *mat, const Vector3D *vec);
Matrix4D* InitTranslationMatrix (Matrix4D *mat, double x, double y, double z);
Matrix4D* InitXRotationMatrix (Matrix4D *mat, double theta);
Matrix4D* InitYRotationMatrix (Matrix4D *mat, double theta);
Matrix4D* InitZRotationMatrix (Matrix4D *mat, double theta);

Matrix4x3* InitMatrix4x3 (Matrix4x3 *mat);
Matrix4x3* MakeMatrix4x3 (Matrix4x3 *dest,
			  const Vector3D *vec1, const Vector3D *vec2,
			  const Vector3D *vec3, const Vector3D *vec4);
Matrix4x3* CopyMatrix4x3 (Matrix4x3 *dest, const Matrix4x3 *src);
Matrix4x3* MultMatrix4x3 (Matrix4x3 *dest,
			  const Matrix4D *mat1, const Matrix4x3 *mat2);
Vector3D* MultVector4DMatrix4x3 (Vector3D *dest,
				 const Vector4D *vec, const Matrix4x3 *mat);

Vector3D* CatmullRom (Vector3D *dest,
		      const Vector3D *vec1, const Vector3D *vec2,
		      const Vector3D *vec3, const Vector3D *vec4,
		      double t);
Matrix4x3* PrecompileCatmullRom (Matrix4x3 *dest,
				 const Vector3D *vec1, const Vector3D *vec2,
				 const Vector3D *vec3, const Vector3D *vec4);
Vector3D* QuickCatmullRom (Vector3D *dest, Matrix4x3 *matC, double t);
