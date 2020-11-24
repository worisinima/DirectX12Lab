#pragma once

#include <Windows.h>
#include <wrl.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>

#include <windows.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <iostream>

using namespace std;
using namespace DirectX;
using namespace DirectX::PackedVector;

//ostream& XM_CALLCONV operator <<(ostream& os, FXMVECTOR v)
//{
//	XMFLOAT3 dest;
//	XMStoreFloat3(&dest, v);
//
//	os << "(" << dest.x << ", " << dest.y << ", " << dest.z << ")";
//	return os;
//}

ostream& XM_CALLCONV operator << (ostream& os, FXMVECTOR v)
{
	XMFLOAT4 dest;
	XMStoreFloat4(&dest, v);

	os << "(" << dest.x << ", " << dest.y << ", " << dest.z << ", " << dest.w << ")";
	return os;
}

ostream& XM_CALLCONV operator << (ostream& os, FXMMATRIX m)
{
	for (int i = 0; i < 4; ++i)
	{
		os << XMVectorGetX(m.r[i]) << "\t";
		os << XMVectorGetY(m.r[i]) << "\t";
		os << XMVectorGetZ(m.r[i]) << "\t";
		os << XMVectorGetW(m.r[i]);
		os << endl;
	}
	return os;
}

int MatrixShowCase()
{
	XMMATRIX A
	(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 2.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 4.0f, 0.0f,
		1.0f, 2.0f, 3.0f, 1.0f
	);

	XMMATRIX B = XMMatrixIdentity();

	XMMATRIX C = A * B;

	XMMATRIX D = XMMatrixTranspose(A);

	XMVECTOR det = XMMatrixDeterminant(A);
	XMMATRIX E = XMMatrixInverse(&det, A);

	XMMATRIX F = A * E;

	cout << "A = " << endl << A << endl;
	cout << "B = " << endl << B << endl;
	cout << "C = A*B = " << endl << C << endl;
	cout << "D = transpose(A) = " << endl << D << endl;
	cout << "det = determinant(A) = " << det << endl << endl;
	cout << "E = inverse(A) = " << endl << E << endl;
	cout << "F = A*E = " << endl << F << endl;

	return 0;
}

int VectorShowCase()
{
	cout.setf(ios_base::boolalpha);

	if (XMVerifyCPUSupport() == false)
	{
		cout << "directx math not supported" << endl;
		return 0;
	}

	XMVECTOR n = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	XMVECTOR u = XMVectorSet(1.0f, 2.0f, 3.0f, 0.0f);
	XMVECTOR v = XMVectorSet(-2.0f, 1.0f, -3.0f, 0.0f);
	XMVECTOR w = XMVectorSet(0.707f, 0.707f, 0.0f, 0.0f);

	// Vector addition: XMVECTOR operator + 
	XMVECTOR a = u + v;

	// Vector subtraction: XMVECTOR operator - 
	XMVECTOR b = u - v;

	// Scalar multiplication: XMVECTOR operator * 
	XMVECTOR c = 10.0f * u;

	// ||u||
	XMVECTOR L = XMVector3Length(u);

	// d = u / ||u||
	XMVECTOR d = XMVector3Normalize(u);

	// s = u dot v
	XMVECTOR s = XMVector3Dot(u, v);

	// e = u cross v
	XMVECTOR e = XMVector3Cross(u, v);

	// Find proj_n(w) and perp_n(w)
	XMVECTOR projW;
	XMVECTOR perpW;
	XMVector3ComponentsFromNormal(&projW, &perpW, w, n);

	// Does projW + perpW == w?
	bool equal = XMVector3Equal(projW + perpW, w) != 0;
	bool notEqual = XMVector3NotEqual(projW + perpW, w) != 0;

	// The angle between projW and perpW should be 90 degrees.
	XMVECTOR angleVec = XMVector3AngleBetweenVectors(projW, perpW);
	float angleRadians = XMVectorGetX(angleVec);
	float angleDegrees = XMConvertToDegrees(angleRadians);

	XMVECTOR ZeroVector = XMVectorZero();
	XMVECTOR SplateOne = XMVectorSplatOne();
	XMVECTOR VectorSet = XMVectorSet(1.0f, 2.0f, 3.0f, 0.0f);
	XMVECTOR Replicate = XMVectorReplicate(-2.0f);
	XMVECTOR SplatZ = XMVectorSplatZ(u);
	XMVECTOR NormlizedVector = XMVector3Normalize(XMVectorSet(1, 2, 3, 1));

	XMVECTOR p = XMVectorSet(2.0f, 2.0f, 1.0f, 0.0f);
	XMVECTOR q = XMVectorSet(2.0f, -0.5f, 0.5f, 0.1f);
	XMVECTOR j = XMVectorSet(1.0f, 2.0f, 4.0f, 8.0f);
	XMVECTOR g = XMVectorSet(-2.0f, 1.0f, -3.0f, 2.5f);
	XMVECTOR f = XMVectorSet(0.0f, XM_PIDIV4, XM_PIDIV2, XM_PI);

	cout << "XMVectorAbs(v)                 = " << XMVectorAbs(v) << endl;
	cout << "XMVectorCos(f)                 = " << XMVectorCos(f) << endl;
	cout << "XMVectorLog(u)                 = " << XMVectorLog(u) << endl;
	cout << "XMVectorExp(p)                 = " << XMVectorExp(p) << endl;

	cout << "XMVectorPow(u, p)              = " << XMVectorPow(u, p) << endl;
	cout << "XMVectorSqrt(u)                = " << XMVectorSqrt(u) << endl;

	cout << "XMVectorSwizzle(u, 2, 2, 1, 3) = " << XMVectorSwizzle(u, 2, 2, 1, 3) << endl;
	cout << "XMVectorSwizzle(u, 2, 1, 0, 3) = " << XMVectorSwizzle(u, 2, 1, 0, 3) << endl;

	cout << "XMVectorMultiply(u, v)         = " << XMVectorMultiply(u, v) << endl;
	cout << "XMVectorSaturate(q)            = " << XMVectorSaturate(q) << endl;
	cout << "XMVectorMin(p, v               = " << XMVectorMin(p, v) << endl;
	cout << "XMVectorMax(p, v)              = " << XMVectorMax(p, v) << endl;

	return 0;
}
