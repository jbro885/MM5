/*
 * Copyright (c) 2003, 2004, 2005
 * The Regents of The University of Michigan
 * All Rights Reserved
 *
 * This code is part of the M5 simulator, developed by Nathan Binkert,
 * Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
 * from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi,
 * and Andrew Schultz.
 *
 * Permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any
 * purpose, so long as the copyright notice above, this grant of
 * permission, and the disclaimer below appear in all copies made; and
 * so long as the name of The University of Michigan is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
 * UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
 * WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE. THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
 * LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGES.
 */


#include <iomanip>
#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>

#include "base/cprintf.hh"
#include "base/misc.hh"
#include "base/statistics.hh"
#include "base/stats/text.hh"
#include "base/stats/mysql.hh"
#include "sim/host.hh"

using namespace std;
using namespace Stats;

Tick curTick = 0;
Tick ticksPerSecond = ULL(2000000000);

Scalar<> s1;
Scalar<> s2;
Average<> s3;
Scalar<MainBin> s4;
Vector<MainBin> s5;
Distribution<MainBin> s6;
Vector<MainBin> s7;
AverageVector<> s8;
StandardDeviation<> s9;
AverageDeviation<> s10;
Scalar<> s11;
Distribution<> s12;
VectorDistribution<> s13;
VectorStandardDeviation<> s14;
VectorAverageDeviation<> s15;
Vector2d<> s16;

Formula f1;
Formula f2;
Formula f3;
Value f4;
Value f5;
Formula f6;
Formula f7;

MainBin bin1("bin1");
MainBin bin2("bin2");

ostream *outputStream = &cout;

double
testfunc()
{
    return 9.8;
}

class TestClass {
  public:
    double operator()() { return 9.7; }
};

char *progname = "";

void
usage()
{
    panic("incorrect usage.\n"
	  "usage:\n"
	  "\t%s [-t [-c] [-d]]\n", progname);
}

int
main(int argc, char *argv[])
{
    bool descriptions = false;
    bool compat = false;
    bool text = false;
    string mysql_name;
    string mysql_host;
    string mysql_user = "binkertn";
    string mysql_passwd;

    char c;
    progname = argv[0];
    while ((c = getopt(argc, argv, "cdh:P:p:s:tu:")) != -1) {
	switch (c) {
	  case 'c':
	    compat = true;
	    break;
	  case 'd':
	    descriptions = true;
	    break;
	  case 'h':
	    mysql_host = optarg;
	    break;
	  case 'P':
	    mysql_passwd = optarg;
	    break;
	  case 's':
	    mysql_name = optarg;
	    break;
	  case 't':
	    text = true;
	    break;
	  case 'u':
	    mysql_user = optarg;
	    break;	    
	  default:
	    usage();
	}
    }

    if (!text && (compat || descriptions))
	usage();

    s5.init(5);
    s6.init(1, 100, 13);
    s7.init(7);
    s8.init(10);
    s12.init(1, 100, 13);
    s13.init(4, 0, 99, 10);
    s14.init(9);
    s15.init(10);
    s16.init(2, 9);

    s1
	.name("Stat01")
	.desc("this is statistic 1")
	;

    s2
	.name("Stat02")
	.desc("this is statistic 2")
	.prereq(s11)
	;

    s3
	.name("Stat03")
	.desc("this is statistic 3")
	.prereq(f7)
	;

    s4
	.name("Stat04")
	.desc("this is statistic 4")
	.prereq(s11)
	;

    s5
	.name("Stat05")
	.desc("this is statistic 5")
	.prereq(s11)
	.subname(0, "foo1")
	.subname(1, "foo2")
	.subname(2, "foo3")
	.subname(3, "foo4")
	.subname(4, "foo5")
	;

    s6
	.name("Stat06")
	.desc("this is statistic 6")
	.prereq(s11)
	;

    s7
	.name("Stat07")
	.desc("this is statistic 7")
	.precision(1)
	.flags(pdf | total)
	.prereq(s11)
	;

    s8
	.name("Stat08")
	.desc("this is statistic 8")
	.precision(2)
	.prereq(s11)
	.subname(4, "blarg")
	;

    s9
	.name("Stat09")
	.desc("this is statistic 9")
	.precision(4)
	.prereq(s11)
	;

    s10
	.name("Stat10")
	.desc("this is statistic 10")
	.prereq(s11)
	;

    s12
	.name("Stat12")
	.desc("this is statistic 12")
	;

    s13
	.name("Stat13")
	.desc("this is statistic 13")
	;

    s14
	.name("Stat14")
	.desc("this is statistic 14")
	;

    s15
	.name("Stat15")
	.desc("this is statistic 15")
	;

    s16
	.name("Stat16")
	.desc("this is statistic 16")
	.flags(total)
	.subname(0, "sub0")
	.subname(1, "sub1")
	.ysubname(0, "y0")
	.ysubname(1, "y1")
	;

    f1
	.name("Formula1")
	.desc("this is formula 1")
	.prereq(s11)
	;

    f2
	.name("Formula2")
	.desc("this is formula 2")
	.prereq(s11)
	.precision(1)
	;

    f3
	.name("Formula3")
	.desc("this is formula 3")
	.prereq(s11)
	.subname(0, "bar1")
	.subname(1, "bar2")
	.subname(2, "bar3")
	.subname(3, "bar4")
	.subname(4, "bar5")
	;

    f4
	.functor(testfunc)
	.name("Formula4")
	.desc("this is formula 4")
	;

    TestClass testclass;
    f5
	.functor(testclass)
	.name("Formula5")
	.desc("this is formula 5")
	;

    f6
	.name("Formula6")
	.desc("this is formula 6")
	;

    f1 = s1 + s2;
    f2 = (-s1) / (-s2) * (-s3 + ULL(100) + s4);
    f3 = sum(s5) * s7;
    f6 += constant(10.0);
    f6 += s5[3];
    f7 = constant(1);

    check();
    reset();

    bin1.activate();

    s16[1][0] = 1;
    s16[0][1] = 3;
    s16[0][0] = 2;
    s16[1][1] = 9;
    s16[1][1] += 9;
    s16[1][8] += 8;
    s16[1][7] += 7;
    s16[1][6] += 6;
    s16[1][5] += 5;
    s16[1][4] += 4;

    s11 = 1;
    s3 = 9;
    s8[3] = 9;
    s15[0].sample(1234);
    s15[1].sample(1234);
    s15[2].sample(1234);
    s15[3].sample(1234);
    s15[4].sample(1234);
    s15[5].sample(1234);
    s15[6].sample(1234);
    s15[7].sample(1234);
    s15[8].sample(1234);
    s15[9].sample(1234);

    s10.sample(1000000000);
    curTick += ULL(1000000);
    s10.sample(100000);
    s10.sample(100000);
    s10.sample(100000);
    s10.sample(100000);
    s10.sample(100000);
    s10.sample(100000);
    s10.sample(100000);
    s10.sample(100000);
    s10.sample(100000);
    s10.sample(100000);
    s10.sample(100000);
    s10.sample(100000);
    s10.sample(100000);
    s13[0].sample(12);
    s13[1].sample(29);
    s13[2].sample(12);
    s13[3].sample(29);
    s13[0].sample(42);
    s13[1].sample(29);
    s13[2].sample(42);
    s13[3].sample(32);
    s13[0].sample(52);
    s13[1].sample(49);
    s13[2].sample(42);
    s13[3].sample(25);
    s13[0].sample(32);
    s13[1].sample(49);
    s13[2].sample(22);
    s13[3].sample(49);
    s13[0].sample(62);
    s13[1].sample(99);
    s13[2].sample(72);
    s13[3].sample(23);
    s13[0].sample(52);
    s13[1].sample(78);
    s13[2].sample(69);
    s13[3].sample(49);

    s14[0].sample(1234);
    s14[1].sample(4134);
    s14[4].sample(1213);
    s14[3].sample(1124);
    s14[2].sample(1243);
    s14[7].sample(1244);
    s14[4].sample(7234);
    s14[2].sample(9234);
    s14[3].sample(1764);
    s14[7].sample(1564);
    s14[3].sample(3234);
    s14[1].sample(2234);
    s14[5].sample(1234);
    s14[2].sample(4334);
    s14[2].sample(1234);
    s14[4].sample(4334);
    s14[6].sample(1234);
    s14[8].sample(8734);
    s14[1].sample(5234);
    s14[3].sample(8234);
    s14[7].sample(5234);
    s14[4].sample(4434);
    s14[3].sample(7234);
    s14[2].sample(1934);
    s14[1].sample(9234);
    s14[5].sample(5634);
    s14[3].sample(1264);
    s14[7].sample(5223);
    s14[0].sample(1234);
    s14[0].sample(5434);
    s14[3].sample(8634);
    s14[1].sample(1234);


    s15[0].sample(1234);
    s15[1].sample(4134);
    curTick += ULL(1000000);
    s15[4].sample(1213);
    curTick += ULL(1000000);
    s15[3].sample(1124);
    curTick += ULL(1000000);
    s15[2].sample(1243);
    curTick += ULL(1000000);
    s15[7].sample(1244);
    curTick += ULL(1000000);
    s15[4].sample(7234);
    s15[2].sample(9234);
    s15[3].sample(1764);
    s15[7].sample(1564);
    s15[3].sample(3234);
    s15[1].sample(2234);
    curTick += ULL(1000000);
    s15[5].sample(1234);
    curTick += ULL(1000000);
    s15[9].sample(4334);
    curTick += ULL(1000000);
    s15[2].sample(1234);
    curTick += ULL(1000000);
    s15[4].sample(4334);
    s15[6].sample(1234);
    curTick += ULL(1000000);
    s15[8].sample(8734);
    curTick += ULL(1000000);
    s15[1].sample(5234);
    curTick += ULL(1000000);
    s15[3].sample(8234);
    curTick += ULL(1000000);
    s15[7].sample(5234);
    s15[4].sample(4434);
    s15[3].sample(7234);
    s15[2].sample(1934);
    s15[1].sample(9234);
    curTick += ULL(1000000);
    s15[5].sample(5634);
    s15[3].sample(1264);
    s15[7].sample(5223);
    s15[0].sample(1234);
    s15[0].sample(5434);
    s15[3].sample(8634);
    curTick += ULL(1000000);
    s15[1].sample(1234);

    s4 = curTick;

    s8[3] = 99999;

    s3 = 12;
    s3++;
    curTick += 9;

    s1 = 9;
    s1 += 9;
    s1 -= 11;
    s1++;
    ++s1;
    s1--;
    --s1;

    s2 = 9;

    s5[0] += 1;
    s5[1] += 2;
    s5[2] += 3;
    s5[3] += 4;
    s5[4] += 5;

    s7[0] = 10;
    s7[1] = 20;
    s7[2] = 30;
    s7[3] = 40;
    s7[4] = 50;
    s7[5] = 60;
    s7[6] = 70;

    s6.sample(0);
    s6.sample(1);
    s6.sample(2);
    s6.sample(3);
    s6.sample(4);
    s6.sample(5);
    s6.sample(6);
    s6.sample(7);
    s6.sample(8);
    s6.sample(9);

    bin2.activate();
    s6.sample(10);
    s6.sample(10);
    s6.sample(10);
    s6.sample(10);
    s6.sample(10);
    s6.sample(10);
    s6.sample(10);
    s6.sample(10);
    s6.sample(11);
    s6.sample(19);
    s6.sample(20);
    s6.sample(20);
    s6.sample(21);
    s6.sample(21);
    s6.sample(31);
    s6.sample(98);
    s6.sample(99);
    s6.sample(99);
    s6.sample(99);

    s7[0] = 700;
    s7[1] = 600;
    s7[2] = 500;
    s7[3] = 400;
    s7[4] = 300;
    s7[5] = 200;
    s7[6] = 100;

    s9.sample(100);
    s9.sample(100);
    s9.sample(100);
    s9.sample(100);
    s9.sample(10);
    s9.sample(10);
    s9.sample(10);
    s9.sample(10);
    s9.sample(10);

    curTick += 9;
    s4 = curTick;
    s6.sample(100);
    s6.sample(100);
    s6.sample(100);
    s6.sample(101);
    s6.sample(102);

    s12.sample(100);

    if (text) {
	Text out(cout);
	out.descriptions = descriptions;
	out.compat = compat;
	out();
    }

    if (!mysql_name.empty()) {
	MySql out;
	out.connect(mysql_host, mysql_user, mysql_passwd, "m5stats",
		    mysql_name, "test");
	out();
    }

    return 0;
}
