#pragma once
#include "Core/ModelTypes.h"
#include "Core/BoundFit.h"
#include <array>
#include <cstring>

namespace workbench {
inline bool CollisionFinite(float v) {
	std::uint32_t bits; std::memcpy(&bits, &v, sizeof(bits));
	return (bits & 0x7f800000u) != 0x7f800000u;
}
inline bool ValidInspectionBound(const TBound& b) {
	if (!CollisionFinite(b.a) || b.a < 0) return false; // Empty remainder may be uninitialized.
	for (float v : {b.cx,b.cy,b.b,b.y1,b.y2,b.cosph,b.sinph,b.gx,b.gz,b.bottomGx,b.bottomGz})
		if (!CollisionFinite(v)) return false;
	return b.a > 0 && b.b > 0 && std::fabs(b.cosph*b.cosph + b.sinph*b.sinph - 1) < .01f;
}
inline Vector3d InspectionCorner(const TBound& b, const Vector3d& origin, int rotation,
	float u, float v, bool top) {
	float x, z;
	c2bound::MapRotate(b.cx + u*b.cosph - v*b.sinph, b.cy + u*b.sinph + v*b.cosph, rotation, x, z);
	return {origin.x+x, origin.y+(top ? c2bound::TopAt(b.y2,b.gx,b.gz,u,v) :
		c2bound::BottomAt(b.y1,b.bottomGx,b.bottomGz,u,v)), origin.z+z};
}
inline std::array<Vector3d,8> InspectionCorners(const TBound& b, const Vector3d& origin, int rotation) {
	std::array<Vector3d,8> corners{};
	for (int i=0;i<4;++i) {
		const float u=(i==0 || i==3) ? -b.a : b.a, v=i<2 ? -b.b : b.b;
		corners[i]=InspectionCorner(b,origin,rotation,u,v,false);
		corners[4+i]=InspectionCorner(b,origin,rotation,u,v,true);
	}
	return corners;
}
struct InspectionCamera {
	float x=0,y=0,z=0, ca=1,sa=0,cb=1,sb=0;
	float cameraW=0,cameraH=0,centerX=0,centerY=0;
	int width=0,height=0;
};
struct InspectionLine { float x1=0,y1=0,x2=0,y2=0; };
inline bool ProjectInspectionLine(const Vector3d& a, const Vector3d& b, const InspectionCamera& c, InspectionLine& line) {
	if(c.width<=0 || c.height<=0 || c.width>16384 || c.height>16384 || c.cameraW<=0 || c.cameraH<=0) return false;
	for(float v:{c.x,c.y,c.z,c.ca,c.sa,c.cb,c.sb,c.cameraW,c.cameraH,c.centerX,c.centerY})if(!CollisionFinite(v))return false;
	const auto view=[&](const Vector3d& p) {
		const float x=p.x-c.x,y=p.y-c.y,z=p.z-c.z;
		const float vx=x*c.ca+z*c.sa, vz=z*c.ca-x*c.sa;
		return Vector3d{vx,y*c.cb-vz*c.sb,-(vz*c.cb+y*c.sb)}; // Positive forward depth.
	};
	auto v1=view(a),v2=view(b);
	for(float v:{v1.x,v1.y,v1.z,v2.x,v2.y,v2.z})if(!CollisionFinite(v))return false;
	constexpr float nearPlane=16; // BuildLegacyProjection's near plane.
	if(v1.z<nearPlane && v2.z<nearPlane)return false;
	if(v1.z<nearPlane || v2.z<nearPlane) {
		const float t=(nearPlane-v1.z)/(v2.z-v1.z);
		const Vector3d hit{v1.x+(v2.x-v1.x)*t,v1.y+(v2.y-v1.y)*t,nearPlane};
		if(v1.z<nearPlane)v1=hit;else v2=hit;
	}
	double x1=c.centerX+static_cast<double>(v1.x)*c.cameraW/v1.z;
	double y1=c.centerY-static_cast<double>(v1.y)*c.cameraH/v1.z;
	double x2=c.centerX+static_cast<double>(v2.x)*c.cameraW/v2.z;
	double y2=c.centerY-static_cast<double>(v2.y)*c.cameraH/v2.z;
	// Clip in double before handing bounded screen coordinates to ImGui.
	const double dx=x2-x1,dy=y2-y1;double low=0,high=1;
	const auto clip=[&](double p,double q) {
		if(p==0)return q>=0;
		const double t=q/p;if(p<0){if(t>high)return false;if(t>low)low=t;}
		else{if(t<low)return false;if(t<high)high=t;}return true;
	};
	if(!clip(-dx,x1)||!clip(dx,c.width-x1)||!clip(-dy,y1)||!clip(dy,c.height-y1))return false;
	line={static_cast<float>(x1+low*dx),static_cast<float>(y1+low*dy),
		static_cast<float>(x1+high*dx),static_cast<float>(y1+high*dy)};
	return CollisionFinite(line.x1)&&CollisionFinite(line.y1)&&CollisionFinite(line.x2)&&CollisionFinite(line.y2) &&
		(line.x1!=line.x2 || line.y1!=line.y2);
}
}
