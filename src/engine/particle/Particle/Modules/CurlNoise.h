#pragma once

#include <cmath>
#include <cstdint>

#include "core/math/Vec3.h"

// ===============================================
// CurlNoise
// カールノイズ用の軽量ユーティリティ。
//
//   Noise3 : 3D value noise（[-1,1]）。外部ライブラリ非依存の自前実装。
//   Curl   : ベクトルポテンシャル Ψ=(ψ1,ψ2,ψ3) の回転 ∇×Ψ を有限差分で計算。
//            ∇·(∇×Ψ)=0 となるため、得られる流れ場は発散ゼロ
//            （divergence-free）で、粒子が溜まらず滑らかに渦を巻く。
// ===============================================
namespace curlnoise {

	// --- ハッシュ（格子点 -> 疑似乱数 [0,1)）---
	inline float Hash(float x, float y, float z) {
		// 格子座標を整数化（Noise3 から floor 済みの値が来るが、負値も安全に扱う）。
		const uint32_t ix = static_cast<uint32_t>(static_cast<int32_t>(std::floor(x)));
		const uint32_t iy = static_cast<uint32_t>(static_cast<int32_t>(std::floor(y)));
		const uint32_t iz = static_cast<uint32_t>(static_cast<int32_t>(std::floor(z)));

		// 大きな素数を掛けて混ぜ、ビット撹拌（finalizer）でハッシュ化。
		// sin を使わないので高速かつ決定的。
		uint32_t h = ix * 0x8da6b343u ^ iy * 0xd8163841u ^ iz * 0xcb1ab31fu;
		h ^= h >> 16;
		h *= 0x7feb352du;
		h ^= h >> 15;
		h *= 0x846ca68bu;
		h ^= h >> 16;

		// 上位 24bit を [0,1) に正規化（float の仮数精度に収める）。
		return (h >> 8) * (1.0f / 16777216.0f); // 16777216 = 2^24
	}

	// --- 5次の補間カーブ（Perlin の fade）---
	inline float Fade(float t) {
		return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
	}

	inline float Lerp(float a, float b, float t) {
		return a + (b - a) * t;
	}

	// --- 3D value noise（出力 [-1,1]）---
	inline float Noise3(const Vec3& p) {
		const float xi = std::floor(p.x);
		const float yi = std::floor(p.y);
		const float zi = std::floor(p.z);
		const float xf = p.x - xi;
		const float yf = p.y - yi;
		const float zf = p.z - zi;

		const float u = Fade(xf);
		const float v = Fade(yf);
		const float w = Fade(zf);

		// 8 つの格子点の値を 3 軸で補間
		const float c000 = Hash(xi,        yi,        zi);
		const float c100 = Hash(xi + 1.0f, yi,        zi);
		const float c010 = Hash(xi,        yi + 1.0f, zi);
		const float c110 = Hash(xi + 1.0f, yi + 1.0f, zi);
		const float c001 = Hash(xi,        yi,        zi + 1.0f);
		const float c101 = Hash(xi + 1.0f, yi,        zi + 1.0f);
		const float c011 = Hash(xi,        yi + 1.0f, zi + 1.0f);
		const float c111 = Hash(xi + 1.0f, yi + 1.0f, zi + 1.0f);

		const float x00 = Lerp(c000, c100, u);
		const float x10 = Lerp(c010, c110, u);
		const float x01 = Lerp(c001, c101, u);
		const float x11 = Lerp(c011, c111, u);
		const float y0  = Lerp(x00, x10, v);
		const float y1  = Lerp(x01, x11, v);
		const float val = Lerp(y0, y1, w);

		return val * 2.0f - 1.0f; // [0,1] -> [-1,1]
	}

	// --- ベクトルポテンシャルの回転（発散ゼロの流れ場）---
	inline Vec3 Curl(const Vec3& p) {
		const float e = 1e-3f; // 有限差分の微小量
		// 3 成分を無相関にするためのオフセット
		const Vec3 o1{ 0.0f,    0.0f,   0.0f };
		const Vec3 o2{ 123.4f, 56.7f,  89.1f };
		const Vec3 o3{ -45.6f, 78.9f, -12.3f };

		auto psi = [&](const Vec3& q) -> Vec3 {
			return Vec3{
				Noise3({ q.x + o1.x, q.y + o1.y, q.z + o1.z }),
				Noise3({ q.x + o2.x, q.y + o2.y, q.z + o2.z }),
				Noise3({ q.x + o3.x, q.y + o3.y, q.z + o3.z }),
			};
		};

		// 各軸方向の前後でポテンシャルをサンプリング
		const Vec3 px0 = psi({ p.x - e, p.y,     p.z     });
		const Vec3 px1 = psi({ p.x + e, p.y,     p.z     });
		const Vec3 py0 = psi({ p.x,     p.y - e, p.z     });
		const Vec3 py1 = psi({ p.x,     p.y + e, p.z     });
		const Vec3 pz0 = psi({ p.x,     p.y,     p.z - e });
		const Vec3 pz1 = psi({ p.x,     p.y,     p.z + e });

		const float inv = 1.0f / (2.0f * e);

		// v_x = ∂ψ3/∂y - ∂ψ2/∂z
		// v_y = ∂ψ1/∂z - ∂ψ3/∂x
		// v_z = ∂ψ2/∂x - ∂ψ1/∂y
		return Vec3{
			((py1.z - py0.z) - (pz1.y - pz0.y)) * inv,
			((pz1.x - pz0.x) - (px1.z - px0.z)) * inv,
			((px1.y - px0.y) - (py1.x - py0.x)) * inv,
		};
	}

} // namespace curlnoise
