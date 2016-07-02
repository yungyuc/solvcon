/*
 * Copyright (c) 2016, Yung-Yu Chen <yyc@solvcon.net>
 * BSD 3-Clause License, see LICENSE.txt
 */

#include <gtest/gtest.h>

#include "march/march.hpp"

using namespace march;
using namespace march::mesh;

TEST(LookupTableTest, Size) {
    LookupTable<index_type, 1> tbl(10, 10);
    EXPECT_EQ(tbl.nbyte(), 80);
    EXPECT_EQ(tbl.nelem(), 20);
    EXPECT_EQ(tbl.nbyte(), tbl.nelem() * sizeof(index_type));
}

TEST(LookupTableTest, Construction) {
    using Type = LookupTable<index_type, 1>;
    EXPECT_NO_THROW(Type(2, 4));
    EXPECT_THROW(Type(-2, 4), std::invalid_argument);
    EXPECT_THROW(Type(2, -4), std::invalid_argument);
    EXPECT_THROW(Type(-2, -4), std::invalid_argument);
}

TEST(LookupTableTest, ConstructionNoOwn) {
    /* When a data pointer is passed to the LookupTable constructor, its Buffer
     * object doesn't manage its own memory! */
    using Type = LookupTable<index_type, 1>;
    char * data = new char[(2+4)*sizeof(index_type)];
    EXPECT_NO_THROW(Type(2, 4));
    delete[] data; // needs to explicitly free the memory.
    // If free again, it should segfault.
    //delete[] data;
}

TEST(LookupTableTest, OutOfRange) {
    LookupTable<index_type, 1> tb1(0, 10), tb2(0, 10);
    EXPECT_THROW(tb2.at(-1), std::out_of_range);
    EXPECT_THROW(tb2.at(10), std::out_of_range);
    EXPECT_NO_THROW(tb2[-1]);
    EXPECT_NO_THROW(tb1[10]);
}

TEST(LookupTableTest, WriteCheckWithOperatorBracket) {
    LookupTable<index_type, 1> tbl(2, 4);
    for (index_type it=-tbl.nghost(); it<tbl.nbody(); ++it) {
        tbl[it][0] = it*10;
    }
    EXPECT_EQ(tbl[-2][0], -20);
    EXPECT_EQ(tbl[-1][0], -10);
    EXPECT_EQ(tbl[0][0], 0);
    EXPECT_EQ(tbl[1][0], 10);
    EXPECT_EQ(tbl[2][0], 20);
    EXPECT_EQ(tbl[3][0], 30);
}

TEST(LookupTableTest, WriteCheckWithAt) {
    LookupTable<index_type, 1> tbl(2, 4);
    for (index_type it=-tbl.nghost(); it<tbl.nbody(); ++it) {
        tbl.at(it)[0] = it*10;
    }
    EXPECT_EQ(tbl.at(-2)[0], -20);
    EXPECT_EQ(tbl.at(-1)[0], -10);
    EXPECT_EQ(tbl.at(0)[0], 0);
    EXPECT_EQ(tbl.at(1)[0], 10);
    EXPECT_EQ(tbl.at(2)[0], 20);
    EXPECT_EQ(tbl.at(3)[0], 30);
}

TEST(LookupTableTest, set) {
    { // no bound check.
        LookupTable<index_type, 2> tbl(2, 4);
        // below causes buffer overflow, don't try!  it's just a demonstration.
        //EXPECT_NO_THROW(tbl.set(-3, -1, -1));
        //EXPECT_NO_THROW(tbl.set( 4, -1, -1));
        for (index_type it=-tbl.nghost(); it<tbl.nbody(); ++it) {
            tbl.set(it, it*10, it*100);
        }
        EXPECT_EQ(tbl[-2][0], -20); EXPECT_EQ(tbl[-2][1], -200);
        EXPECT_EQ(tbl[-1][0], -10); EXPECT_EQ(tbl[-1][1], -100);
        EXPECT_EQ(tbl[ 0][0],   0); EXPECT_EQ(tbl[ 0][1],    0);    ;
        EXPECT_EQ(tbl[ 1][0],  10); EXPECT_EQ(tbl[ 1][1],  100);
        EXPECT_EQ(tbl[ 2][0],  20); EXPECT_EQ(tbl[ 2][1],  200);
        EXPECT_EQ(tbl[ 3][0],  30); EXPECT_EQ(tbl[ 3][1],  300);
    }
    { // with bound check.
        LookupTable<index_type, 2> tbl(2, 4);
        EXPECT_THROW(tbl.set_at(-3, -1, -1), std::out_of_range);
        EXPECT_THROW(tbl.set_at( 4, -1, -1), std::out_of_range);
        for (index_type it=-tbl.nghost(); it<tbl.nbody(); ++it) {
            tbl.set_at(it, it*10, it*100);
        }
        EXPECT_EQ(tbl[-2][0], -20); EXPECT_EQ(tbl[-2][1], -200);
        EXPECT_EQ(tbl[-1][0], -10); EXPECT_EQ(tbl[-1][1], -100);
        EXPECT_EQ(tbl[ 0][0],   0); EXPECT_EQ(tbl[ 0][1],    0);    ;
        EXPECT_EQ(tbl[ 1][0],  10); EXPECT_EQ(tbl[ 1][1],  100);
        EXPECT_EQ(tbl[ 2][0],  20); EXPECT_EQ(tbl[ 2][1],  200);
        EXPECT_EQ(tbl[ 3][0],  30); EXPECT_EQ(tbl[ 3][1],  300);
    }
}

TEST(LookupTableTest, set_row) {
    LookupTable<index_type, 2> tb1(2, 4);
    for (index_type it=-tb1.nghost(); it<tb1.nbody(); ++it) {
        tb1.set(it, it*10, it*100);
    }
    EXPECT_EQ(tb1[-2][0], -20); EXPECT_EQ(tb1[-2][1], -200);
    EXPECT_EQ(tb1[-1][0], -10); EXPECT_EQ(tb1[-1][1], -100);
    EXPECT_EQ(tb1[ 0][0],   0); EXPECT_EQ(tb1[ 0][1],    0);    ;
    EXPECT_EQ(tb1[ 1][0],  10); EXPECT_EQ(tb1[ 1][1],  100);
    EXPECT_EQ(tb1[ 2][0],  20); EXPECT_EQ(tb1[ 2][1],  200);
    EXPECT_EQ(tb1[ 3][0],  30); EXPECT_EQ(tb1[ 3][1],  300);

    LookupTable<index_type, 2> tb2(2, 4);
    for (index_type it=-tb2.nghost(); it<tb2.nbody(); ++it) {
        tb2.set(it, it*10 - 10000, it*100 - 10000);
    }
    EXPECT_NE(tb2[-2][0], -20); EXPECT_NE(tb2[-2][1], -200);
    EXPECT_NE(tb2[-1][0], -10); EXPECT_NE(tb2[-1][1], -100);
    EXPECT_NE(tb2[ 0][0],   0); EXPECT_NE(tb2[ 0][1],    0);    ;
    EXPECT_NE(tb2[ 1][0],  10); EXPECT_NE(tb2[ 1][1],  100);
    EXPECT_NE(tb2[ 2][0],  20); EXPECT_NE(tb2[ 2][1],  200);
    EXPECT_NE(tb2[ 3][0],  30); EXPECT_NE(tb2[ 3][1],  300);

    // OK!  set row by row.
    for (index_type it=-tb2.nghost(); it<tb2.nbody(); ++it) {
        tb2.set(it, tb1.at(it));
    }
    EXPECT_EQ(tb2[-2][0], -20); EXPECT_EQ(tb2[-2][1], -200);
    EXPECT_EQ(tb2[-1][0], -10); EXPECT_EQ(tb2[-1][1], -100);
    EXPECT_EQ(tb2[ 0][0],   0); EXPECT_EQ(tb2[ 0][1],    0);    ;
    EXPECT_EQ(tb2[ 1][0],  10); EXPECT_EQ(tb2[ 1][1],  100);
    EXPECT_EQ(tb2[ 2][0],  20); EXPECT_EQ(tb2[ 2][1],  200);
    EXPECT_EQ(tb2[ 3][0],  30); EXPECT_EQ(tb2[ 3][1],  300);

    // reset for set_at test.
    for (index_type it=-tb2.nghost(); it<tb2.nbody(); ++it) {
        tb2.set(it, it*10 - 10000, it*100 - 10000);
    }
    EXPECT_NE(tb2[-2][0], -20); EXPECT_NE(tb2[-2][1], -200);
    EXPECT_NE(tb2[-1][0], -10); EXPECT_NE(tb2[-1][1], -100);
    EXPECT_NE(tb2[ 0][0],   0); EXPECT_NE(tb2[ 0][1],    0);    ;
    EXPECT_NE(tb2[ 1][0],  10); EXPECT_NE(tb2[ 1][1],  100);
    EXPECT_NE(tb2[ 2][0],  20); EXPECT_NE(tb2[ 2][1],  200);
    EXPECT_NE(tb2[ 3][0],  30); EXPECT_NE(tb2[ 3][1],  300);

    // OK!  set"_at" row by row.
    EXPECT_THROW(tb2.set_at(-3, tb1[0]), std::out_of_range);
    EXPECT_THROW(tb2.set_at( 4, tb1[0]), std::out_of_range);
    for (index_type it=-tb2.nghost(); it<tb2.nbody(); ++it) {
        tb2.set_at(it, tb1.at(it));
    }
    EXPECT_EQ(tb2[-2][0], -20); EXPECT_EQ(tb2[-2][1], -200);
    EXPECT_EQ(tb2[-1][0], -10); EXPECT_EQ(tb2[-1][1], -100);
    EXPECT_EQ(tb2[ 0][0],   0); EXPECT_EQ(tb2[ 0][1],    0);    ;
    EXPECT_EQ(tb2[ 1][0],  10); EXPECT_EQ(tb2[ 1][1],  100);
    EXPECT_EQ(tb2[ 2][0],  20); EXPECT_EQ(tb2[ 2][1],  200);
    EXPECT_EQ(tb2[ 3][0],  30); EXPECT_EQ(tb2[ 3][1],  300);

}

TEST(LookupTableTest, fill) {
    { // 1D table
        LookupTable<index_type, 1> tbl(2, 4);
        tbl.fill(13);
        for (index_type it=-tbl.nghost(); it<tbl.nbody(); ++it) {
            EXPECT_EQ(tbl[it][0], 13);
        }
    }
    { // 2D table
        LookupTable<index_type, 2> tbl(2, 4);
        tbl.fill(-7, 20);
        for (index_type it=-tbl.nghost(); it<tbl.nbody(); ++it) {
            EXPECT_EQ(tbl[it][0], -7); EXPECT_EQ(tbl[it][1], 20);
        }
    }
}

// vim: set ff=unix fenc=utf8 nobomb et sw=4 ts=4:
