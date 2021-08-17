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
  enum class ObjectType {CHeapObject, StackObject, MetaspceObject, ResourceObject, RawCharacters};
  enum class Location {Default, CHeap, Resource, Arena};
  enum class ArrayType {Yes, No};
  enum class StackTracker {Yes, No};
  enum class NullBehaviour {Crash, ReturnNull};

  static void* allocate(size_t size, ObjectType ot, Location location, ArrayType at, NullBehaviour nb, StackTracker st) {
    switch (ot) {
    case ObjectType::CHeapObject: switch (at) {
      case: ArrayType::No: switch (st) {
        case StackTracker::No: switch (nb) {
          case NullBehaviour::Crash:      return CHeapObj<mtTracing>::operator new(size);
          case NullBehaviour::ReturnNull: return CHeapObj<mtTracing>::operator new(size, std::nothrow);
        }
        case StackTracker::Yes: switch (nb) {
          case NullBehaviour::Crash:      return CHeapObj<mtTracing>::operator new(size, CALLER_PC);
          case NullBehaviour::ReturnNull: return CHeapObj<mtTracing>::operator new(size, std::nothrow, CALLER_PC);
        }
      }
      case: ArrayType::Yes: switch (st) {
        case StackTracker::No: switch (nb) {
          case NullBehaviour::Crash:      return CHeapObj<mtTracing>::operator new[](size);
          case NullBehaviour::ReturnNull: return CHeapObj<mtTracing>::operator new[](size, std::nothrow);
        }
        case StackTracker::Yes: switch (nb) {
          case NullBehaviour::Crash:      return CHeapObj<mtTracing>::operator new[](size, CALLER_PC);
          case NullBehaviour::ReturnNull: return CHeapObj<mtTracing>::operator new[](size, std::nothrow, CALLER_PC);
        }
      }
    }

    default:
      printf("");
    }

    return nullptr;
  }
/*
  static void test_allocate() {
    // Add elements
    for (int i = 0; i < 10; i++) {
      a->append(i);
    }

    // Check size
    ASSERT_EQ(a->length(), 10);

    // Check elements
    for (int i = 0; i < 10; i++) {
      EXPECT_EQ(a->at(i), i);
    }
  }
  */
};

TEST_VM_F(AllocationTest, allocate) {
  ASSERT_FALSE(false);
  ASSERT_TRUE(true);
}
