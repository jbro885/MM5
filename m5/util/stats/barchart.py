import matplotlib, pylab
from matplotlib.numerix import array, arange, reshape, shape, transpose, zeros
from matplotlib.numerix import Float

matplotlib.interactive(False)

class BarChart(object):
    def __init__(self, **kwargs):
        self.init(**kwargs)

    def init(self, **kwargs):
        self.colormap = 'jet'
        self.inputdata = None
        self.chartdata = None
        self.xlabel = None
        self.ylabel = None
        self.legend = None
        self.xticks = None
        self.yticks = None
        self.title = None

        for key,value in kwargs.iteritems():
            self.__setattr__(key, value)

    def gen_colors(self, count):
        cmap = matplotlib.cm.get_cmap(self.colormap)
        if count == 1:
            return cmap([ 0.5 ])
        else:
            return cmap(arange(count) / float(count - 1))
        
    # The input data format does not match the data format that the
    # graph function takes because it is intuitive.  The conversion
    # from input data format to chart data format depends on the
    # dimensionality of the input data.  Check here for the
    # dimensionality and correctness of the input data
    def set_data(self, data):
        if data is None:
            self.inputdata = None
            self.chartdata = None
            return

        data = array(data)
        dim = len(shape(data))
        if dim not in (1, 2, 3):
            raise AttributeError, "Input data must be a 1, 2, or 3d matrix"
        self.inputdata = data

        # If the input data is a 1d matrix, then it describes a
        # standard bar chart.
        if dim == 1:
            self.chartdata = array([[data]])

        # If the input data is a 2d matrix, then it describes a bar
        # chart with groups. The matrix being an array of groups of
        # bars.
        if dim == 2:
            self.chartdata = transpose([data], axes=(2,0,1))

        # If the input data is a 3d matrix, then it describes an array
        # of groups of bars with each bar being an array of stacked
        # values.
        if dim == 3:
            self.chartdata = transpose(data, axes=(1,2,0))

    def get_data(self):
        return self.inputdata

    data = property(get_data, set_data)

    # Graph the chart data.
    # Input is a 3d matrix that describes a plot that has multiple
    # groups, multiple bars in each group, and multiple values stacked
    # in each bar.  The underlying bar() function expects a sequence of
    # bars in the same stack location and same group location, so the
    # organization of the matrix is that the inner most sequence
    # represents one of these bar groups, then those are grouped
    # together to make one full stack of bars in each group, and then
    # the outer most layer describes the groups.  Here is an example
    # data set and how it gets plotted as a result.
    #
    # e.g. data = [[[10,11,12], [13,14,15],  [16,17,18], [19,20,21]],
    #              [[22,23,24], [25,26,27],  [28,29,30], [31,32,33]]]
    #
    # will plot like this:
    #
    #    19 31    20 32    21 33
    #    16 28    17 29    18 30
    #    13 25    14 26    15 27
    #    10 22    11 23    12 24
    #
    # Because this arrangement is rather conterintuitive, the rearrange
    # function takes various matricies and arranges them to fit this
    # profile.
    #
    # This code deals with one of the dimensions in the matrix being
    # one wide.
    #
    def graph(self):
        if self.chartdata is None:
            raise AttributeError, "Data not set for bar chart!"

        self.figure = pylab.figure()
        self.axes = self.figure.add_subplot(111)

        dim = len(shape(self.inputdata))
        cshape = shape(self.chartdata)
        if dim == 1:
            colors = self.gen_colors(cshape[2])
            colors = [ [ colors ] * cshape[1] ] * cshape[0]

        if dim == 2:
            colors = self.gen_colors(cshape[0])
            colors = [ [ [ c ] * cshape[2] ] * cshape[1] for c in colors ]

        if dim == 3:
            colors = self.gen_colors(cshape[1])
            colors = [ [ [ c ] * cshape[2] for c in colors ] ] * cshape[0]

        colors = array(colors)

        bars_in_group = len(self.chartdata)
        if bars_in_group < 5:
            width = 1.0 / ( bars_in_group + 1)
            center = width / 2
        else:
            width = .8 / bars_in_group
            center = .1

        bars = []
        for i,stackdata in enumerate(self.chartdata):
            bottom = array([0] * len(stackdata[0]))
            stack = []
            for j,bardata in enumerate(stackdata):
                bardata = array(bardata)
                ind = arange(len(bardata)) + i * width + center
                bar = self.axes.bar(ind, bardata, width, bottom=bottom,
                                    color=colors[i][j])
                stack.append(bar)
                bottom += bardata
            bars.append(stack)

        if self.xlabel is not None:
            self.axes.set_xlabel(self.xlabel)

        if self.ylabel is not None:
            self.axes.set_ylabel(self.ylabel)

        if self.yticks is not None:
            ymin, ymax = self.axes.get_ylim()
            nticks = float(len(self.yticks))
            ticks = arange(nticks) / (nticks - 1) * (ymax - ymin)  + ymin
            self.axes.set_yticks(ticks)
            self.axes.set_yticklabels(self.yticks)

        if self.xticks is not None:
            self.axes.set_xticks(arange(cshape[2]) + .5)
            self.axes.set_xticklabels(self.xticks)

        if self.legend is not None:
            if dim == 1:
                lbars = bars[0][0]
            if dim == 2:
                lbars = [ bars[i][0][0] for i in xrange(len(bars))]
            if dim == 3:
                number = len(bars[0])
                lbars = [ bars[0][number - j - 1][0] for j in xrange(number)]

            self.axes.legend(lbars, self.legend, loc='best')

        if self.title is not None:
            self.axes.set_title(self.title)

    def savefig(self, name):
        self.figure.savefig(name)

if __name__ == '__main__':
    import random, sys

    dim = 3
    number = 5

    args = sys.argv[1:]
    if len(args) > 3:
        sys.exit("invalid number of arguments")
    elif len(args) > 0:
        myshape = [ int(x) for x in args ]
    else:
        myshape = [ 3, 4, 8 ]

    # generate a data matrix of the given shape
    size = reduce(lambda x,y: x*y, myshape)
    #data = [ random.randrange(size - i) + 10 for i in xrange(size) ]
    data = [ float(i)/100.0 for i in xrange(size) ]
    data = reshape(data, myshape)

    # setup some test bar charts
    if True:
        chart1 = BarChart()
        chart1.data = data

        chart1.xlabel = 'Benchmark'
        chart1.ylabel = 'Bandwidth (GBps)'
        chart1.legend = [ 'x%d' % x for x in xrange(myshape[-1]) ]
        chart1.xticks = [ 'xtick%d' % x for x in xrange(myshape[0]) ]
        chart1.title = 'this is the title'
        chart1.graph()
        #chart1.savefig('/tmp/test1.png')

    if False:
        chart2 = BarChart()
        chart2.data = data
        chart2.colormap = 'gray'
        chart2.graph()
        #chart2.savefig('/tmp/test2.png')

    pylab.show()
