/*
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include "precompiled.hpp"
#include "memory/allocation.hpp"
#include "services/memTracker.hpp"

#include "unittest.hpp"
#include <new>

// Test fixture to work with TEST_VM_F
class AllocationTest : public ::testing::Test {
protected:
  enum class ObjectType {CHeapObject, StackObject, MetaspaceObject, ResourceObject, RawCharacters};
  enum class Location {Default, CHeap, Resource, Arena};
  enum class Dimension {Scalar, Vector};
  enum class StackLocation {Default, UserGiven};
  enum class FailureBehaviour {Crash, ReturnNull};
  enum class DeallocationStrategy {DeleteWorks, DeleteFails, DeleteDeleted};

  struct AllocationDeallocationInformation {
    ObjectType _object_type;
    DeallocationStrategy _deallocation_strategy;
    Location _location;
    Dimension _array_type;
    StackLocation _stack_location;
    FailureBehaviour _failure_behaviour;

    AllocationDeallocationInformation() = delete;
    AllocationDeallocationInformation(const AllocationDeallocationInformation&) = default;
    AllocationDeallocationInformation& operator= (const AllocationDeallocationInformation&) = default;

    AllocationDeallocationInformation(
      ObjectType object_type,
      DeallocationStrategy deallocation_strategy,
      Location location,
      Dimension array_type,
      StackLocation stack_location,
      FailureBehaviour failure_behaviour)
      : _object_type(object_type),
        _deallocation_strategy(deallocation_strategy),
        _location(location),
        _array_type(array_type),
        _stack_location(stack_location),
        _failure_behaviour(failure_behaviour) {}
  };

  static AllocationDeallocationInformation versions[];

  static ResourceObj::allocation_type at(Location location) {
    switch (location) {
      case Location::CHeap:    return ResourceObj::C_HEAP;
      case Location::Resource: return ResourceObj::RESOURCE_AREA;
      default:                 return ResourceObj::STACK_OR_EMBEDDED;
    }
  }

  static void* allocate(size_t size, AllocationDeallocationInformation ot) {
    switch (ot._object_type) {
    // ObjectType::CHeapObject
    case ObjectType::CHeapObject: switch (ot._array_type) {
      case Dimension::Scalar: switch (ot._stack_location) {
        case StackLocation::Default: switch (ot._failure_behaviour) {
          case FailureBehaviour::Crash:      return CHeapObj<mtTracing>::operator new(size);
          case FailureBehaviour::ReturnNull: return CHeapObj<mtTracing>::operator new(size, std::nothrow);
        }
        case StackLocation::UserGiven: switch (ot._failure_behaviour) {
          case FailureBehaviour::Crash:      return CHeapObj<mtTracing>::operator new(size, CALLER_PC);
          case FailureBehaviour::ReturnNull: return CHeapObj<mtTracing>::operator new(size, std::nothrow, CALLER_PC);
        }
      }
      case Dimension::Vector: switch (ot._stack_location) {
        case StackLocation::Default: switch (ot._failure_behaviour) {
          case FailureBehaviour::Crash:      return CHeapObj<mtTracing>::operator new[](size);
          case FailureBehaviour::ReturnNull: return CHeapObj<mtTracing>::operator new[](size, std::nothrow);
        }
        case StackLocation::UserGiven: switch (ot._failure_behaviour) {
          case FailureBehaviour::Crash:      return CHeapObj<mtTracing>::operator new[](size, CALLER_PC);
          case FailureBehaviour::ReturnNull: return CHeapObj<mtTracing>::operator new[](size, std::nothrow, CALLER_PC);
        }
      }
    }
    // ObjectType::StackObject
    case ObjectType::StackObject: switch (ot._array_type) {
      case Dimension::Scalar: return StackObj::operator new(size);
      case Dimension::Vector: return StackObj::operator new[](size);
    }
    // ObjectType::MetaspceObject
    case ObjectType::MetaspaceObject: switch ( ot._failure_behaviour) {
      case FailureBehaviour::ReturnNull:     return MetaspaceObj::operator new(size /* unused */, nullptr, size, MetaspaceObj::ConstantPoolType);
      case FailureBehaviour::Crash:          return MetaspaceObj::operator new(size /* unused */, nullptr, size, MetaspaceObj::ConstantPoolType, nullptr /* java thread*/ );
    }
    // ObjectType::ResourceObject
    case ObjectType::ResourceObject: switch (ot._array_type) {
      case Dimension::Scalar: switch (ot._location) {
        case Location::Default: switch (ot._failure_behaviour) {
          case FailureBehaviour::Crash:      return ResourceObj::operator new(size);
          case FailureBehaviour::ReturnNull: return ResourceObj::operator new(size, std::nothrow);
        }
        case Location::CHeap: case Location::Resource: switch (ot._failure_behaviour) {
          case FailureBehaviour::Crash:      return ResourceObj::operator new(size, at(ot._location), mtNone);
          case FailureBehaviour::ReturnNull: return ResourceObj::operator new(size, std::nothrow, at(ot._location), mtNone);
        }
        case Location::Arena: return ResourceObj::operator new(size, nullptr);
      }
      case Dimension::Vector: switch (ot._location) {
        case Location::Default: switch (ot._failure_behaviour) {
          case FailureBehaviour::Crash:      return ResourceObj::operator new[](size);
          case FailureBehaviour::ReturnNull: return ResourceObj::operator new[](size, std::nothrow);
        }
        case Location::CHeap: case Location::Resource: switch (ot._failure_behaviour) {
          case FailureBehaviour::Crash:      return ResourceObj::operator new[](size, at(ot._location), mtNone);
          case FailureBehaviour::ReturnNull: return ResourceObj::operator new[](size, std::nothrow, at(ot._location), mtNone);
        }
        case Location::Arena: return ResourceObj::operator new[](size, nullptr);
      }
    }


    default:
      break;
    }

  //  ASSERT_FALSE("bad configuration used");
    return nullptr;
  }

  static void deallocate(AllocationDeallocationInformation ot, void* ptr) {
      switch (ot._object_type) {
      case ObjectType::CHeapObject: switch (ot._array_type) {
        case Dimension::Scalar: return (void) CHeapObj<mtTracing>::operator delete(ptr);
        case Dimension::Vector: return (void) CHeapObj<mtTracing>::operator delete[](ptr);
      }
      case ObjectType::StackObject: switch (ot._array_type) {
        case Dimension::Scalar: return (void) StackObj::operator delete(ptr);
        case Dimension::Vector: return (void) StackObj::operator delete[](ptr);
      }
      case ObjectType::MetaspaceObject: return (void) MetaspaceObj::operator delete(ptr);
      case ObjectType::ResourceObject: switch (ot._array_type) {
        case Dimension::Scalar: return (void) ResourceObj::operator delete(ptr);
        case Dimension::Vector: return (void) ResourceObj::operator delete[](ptr);
      }
      default:
        break;
      }

    //  ASSERT_FALSE("bad configuration used");
      return;
    }

  static uintptr_t u(void* ptr) {
    return reinterpret_cast<uintptr_t>(ptr);
  }
};

AllocationTest::AllocationDeallocationInformation AllocationTest::versions[] = {
  // CHeapObject
  AllocationDeallocationInformation(ObjectType::CHeapObject, DeallocationStrategy::DeleteWorks, Location::CHeap, Dimension::Vector, StackLocation::UserGiven, FailureBehaviour::Crash),
  AllocationDeallocationInformation(ObjectType::CHeapObject, DeallocationStrategy::DeleteWorks, Location::CHeap, Dimension::Vector, StackLocation::UserGiven, FailureBehaviour::ReturnNull),
  AllocationDeallocationInformation(ObjectType::CHeapObject, DeallocationStrategy::DeleteWorks, Location::CHeap, Dimension::Vector, StackLocation::Default,  FailureBehaviour::Crash),
  AllocationDeallocationInformation(ObjectType::CHeapObject, DeallocationStrategy::DeleteWorks, Location::CHeap, Dimension::Vector, StackLocation::Default,  FailureBehaviour::ReturnNull),
  AllocationDeallocationInformation(ObjectType::CHeapObject, DeallocationStrategy::DeleteWorks, Location::CHeap, Dimension::Scalar,  StackLocation::UserGiven, FailureBehaviour::Crash),
  AllocationDeallocationInformation(ObjectType::CHeapObject, DeallocationStrategy::DeleteWorks, Location::CHeap, Dimension::Scalar,  StackLocation::UserGiven, FailureBehaviour::ReturnNull),
  AllocationDeallocationInformation(ObjectType::CHeapObject, DeallocationStrategy::DeleteWorks, Location::CHeap, Dimension::Scalar,  StackLocation::Default,  FailureBehaviour::Crash),
  AllocationDeallocationInformation(ObjectType::CHeapObject, DeallocationStrategy::DeleteWorks, Location::CHeap, Dimension::Scalar,  StackLocation::Default,  FailureBehaviour::ReturnNull),
  // StackObject
  AllocationDeallocationInformation(ObjectType::StackObject, DeallocationStrategy::DeleteFails, Location::Default, Dimension::Scalar,  StackLocation::Default, FailureBehaviour::Crash),
  AllocationDeallocationInformation(ObjectType::StackObject, DeallocationStrategy::DeleteFails, Location::Default, Dimension::Vector, StackLocation::Default, FailureBehaviour::Crash),
  // MetaspaceObject
  AllocationDeallocationInformation(ObjectType::MetaspaceObject, DeallocationStrategy::DeleteFails, Location::Default, Dimension::Scalar,  StackLocation::Default, FailureBehaviour::Crash),
  AllocationDeallocationInformation(ObjectType::MetaspaceObject, DeallocationStrategy::DeleteFails, Location::Default, Dimension::Scalar,  StackLocation::Default, FailureBehaviour::ReturnNull),
  // ResourceObject
  AllocationDeallocationInformation(ObjectType::ResourceObject, DeallocationStrategy::DeleteFails, Location::Default, Dimension::Scalar, StackLocation::Default, FailureBehaviour::Crash),
  AllocationDeallocationInformation(ObjectType::ResourceObject, DeallocationStrategy::DeleteFails, Location::Default, Dimension::Scalar, StackLocation::Default, FailureBehaviour::ReturnNull),
  AllocationDeallocationInformation(ObjectType::ResourceObject, DeallocationStrategy::DeleteFails, Location::Resource, Dimension::Scalar, StackLocation::Default, FailureBehaviour::Crash),
  AllocationDeallocationInformation(ObjectType::ResourceObject, DeallocationStrategy::DeleteFails, Location::Resource, Dimension::Scalar, StackLocation::Default, FailureBehaviour::ReturnNull),
  AllocationDeallocationInformation(ObjectType::ResourceObject, DeallocationStrategy::DeleteWorks, Location::CHeap, Dimension::Scalar, StackLocation::Default, FailureBehaviour::Crash),
  AllocationDeallocationInformation(ObjectType::ResourceObject, DeallocationStrategy::DeleteWorks, Location::CHeap, Dimension::Scalar, StackLocation::Default, FailureBehaviour::ReturnNull),
  AllocationDeallocationInformation(ObjectType::ResourceObject, DeallocationStrategy::DeleteFails, Location::Arena, Dimension::Scalar, StackLocation::Default, FailureBehaviour::Crash),
  AllocationDeallocationInformation(ObjectType::ResourceObject, DeallocationStrategy::DeleteFails, Location::Default, Dimension::Scalar, StackLocation::Default, FailureBehaviour::Crash),
  AllocationDeallocationInformation(ObjectType::ResourceObject, DeallocationStrategy::DeleteFails, Location::Default, Dimension::Scalar, StackLocation::Default, FailureBehaviour::ReturnNull),
  AllocationDeallocationInformation(ObjectType::ResourceObject, DeallocationStrategy::DeleteFails, Location::Resource, Dimension::Scalar, StackLocation::Default, FailureBehaviour::Crash),
  AllocationDeallocationInformation(ObjectType::ResourceObject, DeallocationStrategy::DeleteFails, Location::Resource, Dimension::Scalar, StackLocation::Default, FailureBehaviour::ReturnNull),
  AllocationDeallocationInformation(ObjectType::ResourceObject, DeallocationStrategy::DeleteWorks, Location::CHeap, Dimension::Scalar, StackLocation::Default, FailureBehaviour::Crash),
  AllocationDeallocationInformation(ObjectType::ResourceObject, DeallocationStrategy::DeleteWorks, Location::CHeap, Dimension::Scalar, StackLocation::Default, FailureBehaviour::ReturnNull),
  AllocationDeallocationInformation(ObjectType::ResourceObject, DeallocationStrategy::DeleteFails, Location::Arena, Dimension::Scalar, StackLocation::Default, FailureBehaviour::Crash)
};

TEST_VM_F(AllocationTest, allocate_and_delete) {
  for (AllocationDeallocationInformation o: AllocationTest::versions) {
    void* ptr = allocate(16, o);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_TRUE((u(ptr) & 0b111) == 0); // 8 bytes alignment

    switch (o._deallocation_strategy) {
    case DeallocationStrategy::DeleteWorks:
      deallocate(o, ptr);
      deallocate(o, nullptr);
      break;
    case DeallocationStrategy::DeleteFails:
      ASSERT_DEATH({
        deallocate(o, ptr);
      }, "delete should fail");
      break;
    case DeallocationStrategy::DeleteDeleted:
      break;
    }
  }
}


