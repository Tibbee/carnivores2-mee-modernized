#pragma once
#include "CollisionGeometry.h"
#include <array>
#include <cstdint>

namespace workbench {
struct CollisionSelection {
	std::uint64_t generation=0;
	int x=-1,z=-1,object=-1,flags=0,rotation=0;
	float distance=0;
};
struct NearbyCollisions {
	std::array<CollisionSelection,81> objects{};
	std::size_t count=0;
};
struct CollisionSnapshot {
	bool valid=false,bounded=false,sceneKnown=false,floorKnown=false,ceilingKnown=false;
	int flags=0,rotation=0;
	Vector3d origin{};
	std::array<TBound,8> bounds{};
	std::array<bool,8> active{};
	float floor=0,ceiling=0,sceneFloor=0,sceneCeiling=0;
};
// Main-thread bounded copied inspection only. No refits, resource pointers,
// collision writes, terrain precache traversal or picking.
NearbyCollisions ReadNearbyCollisions();
CollisionSnapshot ReadCollision(const CollisionSelection& selected, bool queryHeights=true);
InspectionCamera ReadInspectionCamera();
}
