# Copyright (c) 2003, 2004
# The Regents of The University of Michigan
# All Rights Reserved
#
# This code is part of the M5 simulator, developed by Nathan Binkert,
# Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
# from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi,
# and Andrew Schultz.
#
# Permission is granted to use, copy, create derivative works and
# redistribute this software and such derivative works for any purpose,
# so long as the copyright notice above, this grant of permission, and
# the disclaimer below appear in all copies made; and so long as the
# name of The University of Michigan is not used in any advertising or
# publicity pertaining to the use or distribution of this software
# without specific, written prior authorization.
#
# THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
# UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND WITHOUT
# WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER EXPRESS OR
# IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE REGENTS OF
# THE UNIVERSITY OF MICHIGAN SHALL NOT BE LIABLE FOR ANY DAMAGES,
# INCLUDING DIRECT, SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL
# DAMAGES, WITH RESPECT TO ANY CLAIM ARISING OUT OF OR IN CONNECTION
# WITH THE USE OF THE SOFTWARE, EVEN IF IT HAS BEEN OR IS HEREAFTER
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.

import MySQLdb, re, string

def statcmp(a, b):
    v1 = a.split('.')
    v2 = b.split('.')

    last = min(len(v1), len(v2)) - 1
    for i,j in zip(v1[0:last], v2[0:last]):
        if i != j:
            return cmp(i, j)

    # Special compare for last element.
    if len(v1) == len(v2):
        return cmp(v1[last], v2[last])
    else:
        return cmp(len(v1), len(v2))

class RunData:
    def __init__(self, row):
        self.run = int(row[0])
        self.name = row[1]
        self.user = row[2]
        self.project = row[3]

class SubData:
    def __init__(self, row):
        self.stat = int(row[0])
        self.x = int(row[1])
        self.y = int(row[2])
        self.name = row[3]
        self.descr = row[4]

class Data:
    def __init__(self, row):
        if len(row) != 5:
            raise 'stat db error'
        self.stat = int(row[0])
        self.run = int(row[1])
        self.x = int(row[2])
        self.y = int(row[3])
        self.data = float(row[4])

    def __repr__(self):
        return '''Data(['%d', '%d', '%d', '%d', '%f'])''' % ( self.stat,
            self.run, self.x, self.y, self.data)
    
class StatData(object):
    def __init__(self, row):
        self.stat = int(row[0])
        self.name = row[1]
        self.desc = row[2]
        self.type = row[3]
        self.prereq = int(row[5])
        self.precision = int(row[6])

        import flags
        self.flags = 0
        if int(row[4]): self.flags |= flags.printable
        if int(row[7]): self.flags |= flags.nozero
        if int(row[8]): self.flags |= flags.nonan
        if int(row[9]): self.flags |= flags.total
        if int(row[10]): self.flags |= flags.pdf
        if int(row[11]): self.flags |= flags.cdf

        if self.type == 'DIST' or self.type == 'VECTORDIST':
            self.min = float(row[12])
            self.max = float(row[13])
            self.bktsize = float(row[14])
            self.size = int(row[15])

        if self.type == 'FORMULA':
            self.formula = self.db.allFormulas[self.stat]

class Node(object):
    def __init__(self, name):
        self.name = name
    def __str__(self):
        return self.name

class Database(object):
    def __init__(self):
        self.host = 'zizzer.pool'
        self.user = ''
        self.passwd = ''
        self.db = 'm5stats'
        self.cursor = None

        self.allStats = []
        self.allStatIds = {}
        self.allStatNames = {}

        self.allSubData = {}

        self.allRuns = []
        self.allRunIds = {}
        self.allRunNames = {}

        self.allBins = []
        self.allBinIds = {}
        self.allBinNames = {}

        self.allFormulas = {}

        self.stattop = {}
        self.statdict = {}
        self.statlist = []

        self.mode = 'sum';
        self.runs = None
        self.bins = None
        self.ticks = None
        self.__dict__['get'] = type(self).sum

    def query(self, sql):
        self.cursor.execute(sql)

    def update_dict(self, dict):
        dict.update(self.stattop)

    def append(self, stat):
        statname = re.sub(':', '__', stat.name)
        path = string.split(statname, '.')
        pathtop = path[0]
        fullname = ''
        
        x = self
        while len(path) > 1:
            name = path.pop(0)
            if not x.__dict__.has_key(name):
                x.__dict__[name] = Node(fullname + name)
            x = x.__dict__[name]
            fullname = '%s%s.' % (fullname, name)

        name = path.pop(0)
        x.__dict__[name] = stat

        self.stattop[pathtop] = self.__dict__[pathtop]
        self.statdict[statname] = stat
        self.statlist.append(statname)

    def connect(self):
        # connect
        self.thedb = MySQLdb.connect(db=self.db,
                                     host=self.host,
                                     user=self.user,
                                     passwd=self.passwd)

        # create a cursor
        self.cursor = self.thedb.cursor()

        self.query('''select rn_id,rn_name,rn_sample,rn_user,rn_project
                   from runs''')
        for result in self.cursor.fetchall():
            run = RunData(result);
            self.allRuns.append(run)
            self.allRunIds[run.run] = run
            self.allRunNames[run.name] = run

        self.query('select * from bins')
        for id,name in self.cursor.fetchall():
            self.allBinIds[int(id)] = name
            self.allBinNames[name] = int(id)

        self.query('select sd_stat,sd_x,sd_y,sd_name,sd_descr from subdata')
        for result in self.cursor.fetchall():
            subdata = SubData(result)
            if self.allSubData.has_key(subdata.stat):
                self.allSubData[subdata.stat].append(subdata)
            else:
                self.allSubData[subdata.stat] = [ subdata ]

        self.query('select * from formulas')
        for id,formula in self.cursor.fetchall():
            self.allFormulas[int(id)] = formula.tostring()

        StatData.db = self
        self.query('select * from stats')
        import info
        for result in self.cursor.fetchall():
            stat = info.NewStat(StatData(result))
            self.append(stat)
            self.allStats.append(stat)
            self.allStatIds[stat.stat] = stat
            self.allStatNames[stat.name] = stat

    # Name: listbins
    # Desc: Prints all bins matching regex argument, if no argument
    #       is given all bins are returned
    def listBins(self, regex='.*'):
        print '%-50s %-10s' % ('bin name', 'id')
        print '-' * 61
        names = self.allBinNames.keys()
        names.sort()
        for name in names:
            id = self.allBinNames[name]
            print '%-50s %-10d' % (name, id)

    # Name: listruns
    # Desc: Prints all runs matching a given user, if no argument
    #       is given all runs are returned
    def listRuns(self, user=None):
        print '%-40s %-10s %-5s' % ('run name', 'user', 'id')
        print '-' * 62
        for run in self.allRuns:
            if user == None or user == run.user:
                print '%-40s %-10s %-10d' % (run.name, run.user, run.run)

    # Name: listTicks
    # Desc: Prints all samples for a given run
    def listTicks(self, runs=None):
        print "tick"
        print "----------------------------------------"
        sql = 'select distinct dt_tick from data where dt_stat=1180 and (' 
        if runs != None:
            first = True
            for run in runs:
               if first:
            #       sql += ' where'
                   first = False
               else:
                   sql += ' or' 
               sql += ' dt_run=%s' % run.run 
            sql += ')'
        self.query(sql)
        for r in self.cursor.fetchall():
            print r[0]

    # Name: retTicks
    # Desc: Prints all samples for a given run
    def retTicks(self, runs=None):
        sql = 'select distinct dt_tick from data where dt_stat=1180 and (' 
        if runs != None:
            first = True
            for run in runs:
               if first:
                   first = False
               else:
                   sql += ' or' 
               sql += ' dt_run=%s' % run.run 
            sql += ')'
        self.query(sql)
        ret = []
        for r in self.cursor.fetchall():
            ret.append(r[0])
        return ret

    # Name: liststats
    # Desc: Prints all statistics that appear in the database,
    #         the optional argument is a regular expression that can
    #         be used to prune the result set
    def listStats(self, regex=None):
        print '%-60s %-8s %-10s' % ('stat name', 'id', 'type')
        print '-' * 80

        rx = None
        if regex != None:
            rx = re.compile(regex)

        stats = [ stat.name for stat in self.allStats ]
        stats.sort(statcmp)
        for stat in stats:
            stat = self.allStatNames[stat]
            if rx == None or rx.match(stat.name):
                print '%-60s %-8s %-10s' % (stat.name, stat.stat, stat.type)

    # Name: liststats
    # Desc: Prints all statistics that appear in the database,
    #         the optional argument is a regular expression that can
    #         be used to prune the result set
    def listFormulas(self, regex=None):
        print '%-60s %s' % ('formula name', 'formula')
        print '-' * 80

        rx = None
        if regex != None:
            rx = re.compile(regex)

        stats = [ stat.name for stat in self.allStats ]
        stats.sort(statcmp)
        for stat in stats:
            stat = self.allStatNames[stat]
            if stat.type == 'FORMULA' and (rx == None or rx.match(stat.name)):
                print '%-60s %s' % (stat.name, self.allFormulas[stat.stat])

    def getStat(self, stats):
        if type(stats) is not list:
            stats = [ stats ]

        ret = []
        for stat in stats:
            if type(stat) is int:
                ret.append(self.allStatIds[stat])

            if type(stat) is str:
                rx = re.compile(stat)
                for stat in self.allStats:
                    if rx.match(stat.name):
                        ret.append(stat)
        return ret
            
    def getBin(self, bins):
        if type(bins) is not list:
            bins = [ bins ]

        ret = []
        for bin in bins:
            if type(bin) is int:
                ret.append(bin)
            elif type(bin) is str:
                ret.append(self.allBinNames[bin])
            else:
                for name,id in self.allBinNames.items():
                    if bin.match(name):
                        ret.append(id)

        return ret

    def getNotBin(self, bin):
        map = {}
        for bin in getBin(bin):
            map[bin] = 1

        ret = []
        for bin in self.allBinIds.keys():
            if not map.has_key(bin):
                ret.append(bin)
    
        return ret

    #########################################
    # get the data
    #
    def inner(self, op, stat, bins, ticks, group=False):
        sql = 'select '
        sql += 'dt_stat as stat, '
        sql += 'dt_run as run, '
        sql += 'dt_x as x, '
        sql += 'dt_y as y, '
        if group:
            sql += 'dt_tick as tick, '
        sql += '%s(dt_data) as data ' % op
        sql += 'from data '
        sql += 'where '

        if isinstance(stat, list):
            val = ' or '.join([ 'dt_stat=%d' % s.stat for s in stat ])
            sql += ' (%s)' % val
        else:
            sql += ' dt_stat=%d' % stat.stat
        
        if self.runs != None and len(self.runs):
            val = ' or '.join([ 'dt_run=%d' % r for r in self.runs ])
            sql += ' and (%s)' % val

        if bins != None and len(bins):
            val = ' or '.join([ 'dt_bin=%d' % b for b in bins ])
            sql += ' and (%s)' % val

        if ticks != None and len(ticks):
            val = ' or '.join([ 'dt_tick=%d' % s for s in ticks ])
            sql += ' and (%s)' % val

        sql += ' group by dt_stat,dt_run,dt_x,dt_y'
        if group:
            sql += ',dt_tick'
        return sql

    def outer(self, op_out, op_in, stat, bins, ticks):
        sql = self.inner(op_in, stat, bins, ticks, True)
        sql = 'select stat,run,x,y,%s(data) from (%s) as tb ' % (op_out, sql)
        sql += 'group by stat,run,x,y'
        return sql

    # Name: sum
    # Desc: given a run, a stat and an array of samples and bins,
    #        sum all the bins and then get the standard deviation of the
    #        samples for non-binned runs. This will just return the average
    #        of samples, however a bin array still must be passed
    def sum(self, stat, bins, ticks):
        return self.inner('sum', stat, bins, ticks)

    # Name: avg
    # Desc: given a run, a stat and an array of samples and bins,
    #        sum all the bins and then average the samples for non-binned
    #        runs this will just return the average of samples, however
    #        a bin array still must be passed
    def avg(self, stat, bins, ticks):
        return self.outer('avg', 'sum', stat, bins, ticks)

    # Name: stdev
    # Desc: given a run, a stat and an array of samples and bins,
    #        sum all the bins and then get the standard deviation of the
    #        samples for non-binned runs. This will just return the average
    #        of samples, however a bin array still must be passed
    def stdev(self, stat, bins, ticks):
        return self.outer('stddev', 'sum', stat, bins, ticks)

    def __getattribute__(self, attr):
        if attr != 'get':
            return super(Database, self).__getattribute__(attr)

        if self.__dict__['get'] == type(self).sum:
            return 'sum'
        elif self.__dict__['get'] == type(self).avg:
            return 'avg'
        elif self.__dict__['get'] == type(self).stdev:
            return 'stdev'
        else:
            return ''

    def __setattr__(self, attr, value):
        if attr != 'get':
            super(Database, self).__setattr__(attr, value)
            return

        if value == 'sum':
            self.__dict__['get'] = type(self).sum
        elif value == 'avg':
            self.__dict__['get'] = type(self).avg
        elif value == 'stdev':
            self.__dict__['get'] = type(self).stdev
        else:
            raise AttributeError, "can only set get to: sum | avg | stdev"

    def data(self, stat, bins=None, ticks=None):
        if bins is None:
            bins = self.bins
        if ticks is None:
            ticks = self.ticks
        sql = self.__dict__['get'](self, stat, bins, ticks)
        self.query(sql)

        runs = {}
        for x in self.cursor.fetchall():
            data = Data(x)
            if not runs.has_key(data.run):
                runs[data.run] = {}
            if not runs[data.run].has_key(data.x):
                runs[data.run][data.x] = {}

            runs[data.run][data.x][data.y] = data.data
        return runs

    def __getitem__(self, key):
        return self.stattop[key]
