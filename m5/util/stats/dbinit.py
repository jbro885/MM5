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

import MySQLdb

class MyDB(object):
    def __init__(self, options):
        self.name = options.db
        self.host = options.host
        self.user = options.user
        self.passwd = options.passwd
        self.mydb = None
        self.cursor = None

    def admin(self):
        self.close()
        self.mydb = MySQLdb.connect(db='mysql', host=self.host, user=self.user,
                                    passwd=self.passwd)
        self.cursor = self.mydb.cursor()

    def connect(self):
        self.close()
        self.mydb = MySQLdb.connect(db=self.name, host=self.host,
                                    user=self.user, passwd=self.passwd)
        self.cursor = self.mydb.cursor()

    def close(self):
        if self.mydb is not None:
            self.mydb.close()
        self.cursor = None

    def query(self, sql):
        self.cursor.execute(sql)

    def drop(self):
        self.query('DROP DATABASE IF EXISTS %s' % self.name)

    def create(self):
        self.query('CREATE DATABASE %s' % self.name)

    def populate(self):
        #
        # Each run (or simulation) gets its own entry in the runs table to
        # group stats by where they were generated
        #
        # COLUMNS:
        #   'id' is a unique identifier for each run to be used in other
        #       tables.
        #   'name' is the user designated name for the data generated.  It is
        #       configured in the simulator.
        #   'user' identifies the user that generated the data for the given
        #       run.
        #   'project' another name to identify runs for a specific goal
        #   'date' is a timestamp for when the data was generated.  It can be
        #       used to easily expire data that was generated in the past.
        #   'expire' is a timestamp for when the data should be removed from
        #       the database so we don't have years worth of junk.
        #  
        # INDEXES:
        #   'run' is indexed so you can find out details of a run if the run
        #       was retreived from the data table. 
        #   'name' is indexed so that two all run names are forced to be unique
        #
        self.query('''
        CREATE TABLE runs(
	    rn_id	SMALLINT UNSIGNED	NOT NULL AUTO_INCREMENT,
	    rn_name	VARCHAR(200)		NOT NULL,
	    rn_sample	VARCHAR(32)		NOT NULL,
	    rn_user	VARCHAR(32)		NOT NULL,
            rn_project	VARCHAR(100)            NOT NULL,
	    rn_date	TIMESTAMP		NOT NULL,
            rn_expire	TIMESTAMP               NOT NULL,
	    PRIMARY KEY (rn_id),
	    UNIQUE (rn_name,rn_sample)
        ) TYPE=InnoDB''')

        #
        # We keep the bin names separate so that the data table doesn't get
        # huge since bin names are frequently repeated.
        #
        # COLUMNS:
        #   'id' is the unique bin identifer.
        #   'name' is the string name for the bin.
        #
        # INDEXES:
        #   'bin' is indexed to get the name of a bin when data is retrieved
        #       via the data table.
        #   'name' is indexed to get the bin id for a named bin when you want
        #       to search the data table based on a specific bin.
        #
        self.query('''
        CREATE TABLE bins(
            bn_id	SMALLINT UNSIGNED	NOT NULL AUTO_INCREMENT,
            bn_name	VARCHAR(255)		NOT NULL,
            PRIMARY KEY(bn_id),
            UNIQUE (bn_name)
        ) TYPE=InnoDB''')

        #
        # The stat table gives us all of the data for a particular stat.
        #
        # COLUMNS:
        #   'stat' is a unique identifier for each stat to be used in other
        #       tables for references.
        #   'name' is simply the simulator derived name for a given
        #       statistic. 
        #   'descr' is the description of the statistic and what it tells
        #       you. 
        #   'type' defines what the stat tells you.  Types are:
        #       SCALAR: A simple scalar statistic that holds one value
        #       VECTOR: An array of statistic values.  Such a something that
        #           is generated per-thread.  Vectors exist to give averages,
        #	     pdfs, cdfs, means, standard deviations, etc across the
        #           stat values. 
        #       DIST: Is a distribution of data.  When the statistic value is
        #	     sampled, its value is counted in a particular bucket.
        #           Useful for keeping track of utilization of a resource.
        #           (e.g. fraction of time it is 25% used vs. 50% vs. 100%)
        #       VECTORDIST: Can be used when the distribution needs to be
        #	     factored out into a per-thread distribution of data for
        #	     example.  It can still be summed across threads to find
        #           the total distribution.
        #       VECTOR2D: Can be used when you have a stat that is not only
        #           per-thread, but it is per-something else.  Like
        #           per-message type.
        #       FORMULA: This statistic is a formula, and its data must be
        #	     looked up in the formula table, for indicating how to
        #           present its values.
        #   'subdata' is potentially used by any of the vector types to 
        #       give a specific name to all of the data elements within a
        #       stat.
        #   'print' indicates whether this stat should be printed ever.
        #       (Unnamed stats don't usually get printed)  
        #   'prereq' only print the stat if the prereq is not zero.
        #   'prec' number of decimal places to print
        #   'nozero' don't print zero values
        #   'nonan' don't print NaN values
        #   'total' for vector type stats, print the total.
        #   'pdf' for vector type stats, print the pdf.
        #   'cdf' for vector type stats, print the cdf.
        #
        #   The Following are for dist type stats:
        #   'min' is the minimum bucket value. Anything less is an underflow. 
        #   'max' is the maximum bucket value. Anything more is an overflow.
        #   'bktsize' is the approximate number of entries in each bucket.
        #   'size' is the number of buckets. equal to (min/max)/bktsize.
        #
        # INDEXES:
        #   'stat' is indexed so that you can find out details about a stat
        #       if the stat id was retrieved from the data table.
        #   'name' is indexed so that you can simply look up data about a
        #       named stat.
        #
        self.query('''
        CREATE TABLE stats(
            st_id	SMALLINT UNSIGNED	NOT NULL AUTO_INCREMENT,
            st_name	VARCHAR(255)		NOT NULL,
            st_descr	TEXT			NOT NULL,
            st_type	ENUM("SCALAR", "VECTOR", "DIST", "VECTORDIST",
                "VECTOR2D", "FORMULA")	NOT NULL,
            st_print	BOOL			NOT NULL,
            st_prereq	SMALLINT UNSIGNED	NOT NULL,
            st_prec	TINYINT			NOT NULL,
            st_nozero	BOOL			NOT NULL,
            st_nonan	BOOL			NOT NULL,
            st_total	BOOL			NOT NULL,
            st_pdf	BOOL			NOT NULL,
            st_cdf	BOOL			NOT NULL,
            st_min	DOUBLE			NOT NULL,
            st_max	DOUBLE			NOT NULL,
            st_bktsize	DOUBLE			NOT NULL,
            st_size	SMALLINT UNSIGNED	NOT NULL,
            PRIMARY KEY (st_id),
            UNIQUE (st_name)
        ) TYPE=InnoDB''')

        #
        # This is the main table of data for stats.
        #
        # COLUMNS:
        #   'stat' refers to the stat field given in the stat table.
        #
        #   'x' referrs to the first dimension of a multi-dimensional stat. For
        #       a vector, x will start at 0 and increase for each vector
        #       element.
        #       For a distribution:
        #       -1: sum (for calculating standard deviation)
        #       -2: sum of squares (for calculating standard deviation)
        #       -3: total number of samples taken (for calculating
        #           standard deviation)
        #       -4: minimum value
        #       -5: maximum value
        #       -6: underflow
        #       -7: overflow
        #   'y' is used by a VECTORDIST and the VECTOR2D to describe the second
        #       dimension.
        #   'run' is the run that the data was generated from.  Details up in
        #       the run table
        #   'tick' is a timestamp generated by the simulator.
        #   'bin' is the name of the bin that the data was generated in, if
        #       any.
        #   'data' is the actual stat value.
        #
        # INDEXES:
        #   'stat' is indexed so that a user can find all of the data for a
        #       particular stat. It is not unique, because that specific stat
        #       can be found in many runs, bins, and samples, in addition to
        #       having entries for the mulidimensional cases. 
        #   'run' is indexed to allow a user to remove all of the data for a
        #       particular execution run.  It can also be used to allow the
        #       user to print out all of the data for a given run.
        #
        self.query('''
        CREATE TABLE data(
            dt_stat	SMALLINT UNSIGNED	NOT NULL,
            dt_x	SMALLINT		NOT NULL,
            dt_y	SMALLINT		NOT NULL,
            dt_run	SMALLINT UNSIGNED	NOT NULL,
            dt_tick	BIGINT UNSIGNED		NOT NULL,
            dt_bin	SMALLINT UNSIGNED	NOT NULL,
            dt_data	DOUBLE			NOT NULL, 
            INDEX (dt_stat),
            INDEX (dt_run),
            UNIQUE (dt_stat,dt_x,dt_y,dt_run,dt_tick,dt_bin)
        ) TYPE=InnoDB;''')

        #
        # Names and descriptions for multi-dimensional stats (vectors, etc.)
        # are stored here instead of having their own entry in the statistics
        # table. This allows all parts of a single stat to easily share a
        # single id. 
        #
        # COLUMNS:
        #   'stat' is the unique stat identifier from the stat table.
        #   'x' is the first dimension for multi-dimensional stats
        #       corresponding to the data table above.
        #   'y' is the second dimension for multi-dimensional stats
        #       corresponding to the data table above.
        #   'name' is the specific subname for the unique stat,x,y combination.
        #   'descr' is the specific description for the uniqe stat,x,y
        #        combination.
        #
        # INDEXES:
        #   'stat' is indexed so you can get the subdata for a specific stat.
        #
        self.query('''
        CREATE TABLE subdata(
            sd_stat	SMALLINT UNSIGNED	NOT NULL,
            sd_x	SMALLINT		NOT NULL,
            sd_y	SMALLINT		NOT NULL,
            sd_name	VARCHAR(255)		NOT NULL,
            sd_descr	TEXT,
            UNIQUE (sd_stat,sd_x,sd_y)
        ) TYPE=InnoDB''')


        #
        # The formula table is maintained separately from the data table
        # because formula data, unlike other stat data cannot be represented
        # there.
        #
        # COLUMNS:
        #   'stat' refers to the stat field generated in the stat table.
        #   'formula' is the actual string representation of the formula
        #       itself.
        #
        # INDEXES:
        #   'stat' is indexed so that you can just look up a formula.
        #
        self.query('''
        CREATE TABLE formulas(
            fm_stat	SMALLINT UNSIGNED	NOT NULL,
            fm_formula	BLOB			NOT NULL,
            PRIMARY KEY(fm_stat)
        ) TYPE=InnoDB''')

        #
        # Each stat used in each formula is kept in this table.  This way, if
        # you want to print out a particular formula, you can simply find out
        # which stats you need by looking in this table.  Additionally, when
        # you remove a stat from the stats table and data table, you remove
        # any references to the formula in this table.  When a formula is no
        # longer referred to, you remove its entry.
        #
        # COLUMNS:
        #   'stat' is the stat id from the stat table above.
        #   'child' is the stat id of a stat that is used for this formula.
        #       There may be many children for any given 'stat' (formula)
        #
        # INDEXES:
        #   'stat' is indexed so you can look up all of the children for a
        #       particular stat.
        #   'child' is indexed so that you can remove an entry when a stat is
        #       removed.
        #
        self.query('''
        CREATE TABLE formula_ref(
            fr_stat	SMALLINT UNSIGNED	NOT NULL,
            fr_run	SMALLINT UNSIGNED	NOT NULL,
            UNIQUE (fr_stat,fr_run),
            INDEX (fr_stat),
            INDEX (fr_run)
        ) TYPE=InnoDB''')

        # COLUMNS:
        #   'event' is the unique event id from the event_desc table
        #   'run' is simulation run id that this event took place in
        #   'tick' is the tick when the event happened
        #
        # INDEXES:
        #   'event' is indexed so you can look up all occurences of a
        #       specific event
        #   'run' is indexed so you can find all events in a run
        #   'tick' is indexed because we want the unique thing anyway
        #   'event,run,tick' is unique combination
        self.query('''
        CREATE TABLE events(
            ev_event	SMALLINT UNSIGNED	NOT NULL,
            ev_run	SMALLINT UNSIGNED	NOT NULL,
            ev_tick	BIGINT   UNSIGNED       NOT NULL,
            INDEX(ev_event),
            INDEX(ev_run),
            INDEX(ev_tick),
            UNIQUE(ev_event,ev_run,ev_tick)
        ) TYPE=InnoDB''')

        # COLUMNS:
        #   'id' is the unique description id
        #   'name' is the name of the event that occurred
        #
        # INDEXES:
        #   'id' is indexed because it is the primary key and is what you use
        #       to look up the descriptions
        #   'name' is indexed so one can find the event based on name
        #
        self.query('''
        CREATE TABLE event_names(
            en_id	SMALLINT UNSIGNED	NOT NULL AUTO_INCREMENT,
            en_name	VARCHAR(255)		NOT NULL,
            PRIMARY KEY (en_id),
            UNIQUE (en_name)
        ) TYPE=InnoDB''')

    def clean(self):
        self.query('''
        DELETE data
        FROM data
        LEFT JOIN runs ON dt_run=rn_id
        WHERE rn_id IS NULL''')

        self.query('''
        DELETE formula_ref
        FROM formula_ref
        LEFT JOIN runs ON fr_run=rn_id
        WHERE rn_id IS NULL''')

        self.query('''
        DELETE formulas
        FROM formulas
        LEFT JOIN formula_ref ON fm_stat=fr_stat
        WHERE fr_stat IS NULL''')

        self.query('''
        DELETE stats
        FROM stats
        LEFT JOIN data ON st_id=dt_stat
        WHERE dt_stat IS NULL''')

        self.query('''
        DELETE subdata
        FROM subdata
        LEFT JOIN data ON sd_stat=dt_stat
        WHERE dt_stat IS NULL''')

        self.query('''
        DELETE bins
        FROM bins
        LEFT JOIN data ON bn_id=dt_bin
        WHERE dt_bin IS NULL''')

        self.query('''
        DELETE events
        FROM events
        LEFT JOIN runs ON ev_run=rn_id
        WHERE rn_id IS NULL''')

        self.query('''
        DELETE event_names
        FROM event_names
        LEFT JOIN events ON en_id=ev_event
        WHERE ev_event IS NULL''')
