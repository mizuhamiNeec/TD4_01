#pragma once
#include <memory>
#include <engine/Line/Line.h>
#include <runtime/core/math/Math.h>

#include "engine/unnamed/uprimitive/UPrimitives.h"

/// @brief デバッグ描画クラス
class Debug {
public:
	static void DrawLine(Vec3 a, Vec3 b, const Vec4& color);
	static void DrawRay(const Vec3& position, const Vec3& dir,
	                    const Vec4& color);
	static void DrawAxis(const Vec3& position, const Quaternion& orientation);
	static void DrawCircle(
		const Vec3& position, const Quaternion& rotation, float radius,
		const Vec4& color, uint32_t             segments = 32
	);
	static void DrawArc(
		float             startAngle, float endAngle, const Vec3& position,
		const Quaternion& orientation, float radius, const Vec4& color,
		bool              drawChord   = false, bool drawSector = false,
		int               arcSegments = 32
	);
	static void DrawArrow(const Vec3& position, const Vec3& direction,
	                      const Vec4& color, float          headSize = 0.25f);
	static void DrawQuad(
		const Vec3& pointA, const Vec3& pointB, const Vec3& pointC,
		const Vec3& pointD, const Vec4& color
	);
	static void DrawRect(const Vec3& position, const Quaternion& orientation,
	                     const Vec2& extent, const Vec4&         color);
	static void DrawRect(
		const Vec2&       point1, const Vec2&      point2, const Vec3& origin,
		const Quaternion& orientation, const Vec4& color
	);
	static void DrawSphere(
		const Vec3& position, const Quaternion& orientation, float radius,
		const Vec4& color, int                  segments = 8
	);
	static void DrawBox(const Vec3& position, const Quaternion& orientation,
	                    Vec3        size, const Vec4&           color);
	static void DrawCylinder(
		const Vec3& position, const Quaternion& orientation,
		float       height,
		float       radius, const Vec4& color, bool drawFromBase = true
	);
	static void DrawCapsule(
		const Vec3& position, const Quaternion& orientation,
		float       height,
		float       radius, const Vec4& color, bool drawFromBase = true
	);
	static void DrawCapsule(
		const Vec3& start, const Vec3& end, float radius,
		const Vec4& color
	);
	static void DrawTriangle(const Unnamed::Triangle& triangle, Vec4 color);

	static void Init(LineCommon* lineCommon);
	static void Update();
	static void Draw();
	static void Shutdown();

private:
	static std::unique_ptr<Line> mLine;
};
