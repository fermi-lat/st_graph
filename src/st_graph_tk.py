"""st_graph_tk.py
   Tk/matplotlib implementation of the graphical objects used by the st_graph C++ library
   (MPLEngine, MPLFrame, MPLPlotFrame and MPLTabFolder).

   Each C++ MPLFrame owns exactly one wrapper object from this module. Wrappers keep their own
   geometry (position relative to the parent frame, width and height) and apply it with Tk's place
   geometry manager. This mirrors the way ROOT's TGFrame objects behave, so the C++ layout code
   (st_graph::Placer and IEventReceiver::layout) can position widgets explicitly.

   Events are reported back to C++ by calling the callback supplied when the wrapper was created,
   with an event name followed by event-specific arguments:
     ('clicked',)                         button or check button pressed
     ('modified', text)                   contents of a text entry changed
     ('close',)                           user asked the window manager to close a main frame
     ('layout',)                          main frame was resized and needs to be laid out again
     ('rightClicked', x, y)               right mouse button released inside the axes of a 2d plot
     ('markerMoved', plot, marker, x, y)  user dragged a plot marker to a new position

   Environment variables (intended for testing):
     ST_GRAPH_AUTO_CLOSE_MS  if set to a positive number of milliseconds, each call to mainloop()
                             returns automatically after that delay.
     ST_GRAPH_SNAPSHOT_DIR   if set, every time a plot is drawn a PNG image of it is written into
                             this directory.
"""
import os
import tkinter as tk
from tkinter import filedialog, ttk

_root = None
_snapshot_count = 0

_COLORS = ['w', 'k', 'r', 'g', 'b', 'y', 'm', 'c']
_LINE_STYLES = {'solid': '-', 'dashed': '--', 'dotted': ':', 'none': 'none'}


def init():
    """Create (once) and return the hidden Tk root window. Raises tkinter.TclError if no display is available."""
    global _root
    if _root is None:
        _root = tk.Tk(baseName='st_graph', className='St_graph')
        _root.withdraw()
    return _root


def mainloop():
    """Run the Tk event loop until quit() is called."""
    root = init()
    job = None
    try:
        auto_close = int(os.environ.get('ST_GRAPH_AUTO_CLOSE_MS', '0'))
    except ValueError:
        auto_close = 0
    if auto_close > 0:
        job = root.after(auto_close, root.quit)
    try:
        root.mainloop()
    finally:
        if job is not None:
            root.after_cancel(job)


def quit():
    """Cause the innermost running mainloop() to return."""
    if _root is not None:
        _root.quit()


def color_name(color):
    """Convert an st_graph::Color code to a matplotlib color."""
    if 0 <= color < len(_COLORS):
        return _COLORS[color]
    return 'k'


def file_dialog(parent, directory, file_name, style):
    """Display a modal file dialog box. Return the chosen file name, or an empty string if canceled."""
    master = parent.widget.winfo_toplevel() if parent is not None and parent.widget is not None else init()
    options = {'parent': master}
    if directory:
        options['initialdir'] = directory
    if file_name:
        options['initialfile'] = file_name
    if style == 'open':
        result = filedialog.askopenfilename(**options)
    else:
        result = filedialog.asksaveasfilename(**options)
    return result if isinstance(result, str) else ''


class _ToolTip(object):
    """Small yellow window which pops up when the pointer rests over a widget."""

    def __init__(self, widget, text):
        self.widget = widget
        self.text = text
        self.tip = None
        self.job = None
        widget.bind('<Enter>', self._schedule, add='+')
        widget.bind('<Leave>', self.hide, add='+')
        widget.bind('<ButtonPress>', self.hide, add='+')

    def _schedule(self, event=None):
        self._cancel()
        self.job = self.widget.after(500, self._show)

    def _cancel(self):
        if self.job is not None:
            self.widget.after_cancel(self.job)
            self.job = None

    def _show(self):
        self.job = None
        if not self.text or self.tip is not None:
            return
        x = self.widget.winfo_rootx() + 10
        y = self.widget.winfo_rooty() + self.widget.winfo_height() + 5
        self.tip = tk.Toplevel(self.widget)
        self.tip.wm_overrideredirect(True)
        self.tip.wm_geometry('+%d+%d' % (x, y))
        tk.Label(self.tip, text=self.text, justify='left', background='#ffffe0', relief='solid',
                 borderwidth=1).pack()

    def hide(self, event=None):
        self._cancel()
        if self.tip is not None:
            try:
                self.tip.destroy()
            except tk.TclError:
                pass
            self.tip = None


class Ancestor(object):
    """Stand-in for the invisible frame which is the parent of all main frames."""

    def __init__(self):
        self.widget = init()

    def container(self):
        return self.widget

    def geometry(self):
        return (0, 0, 0, 0)

    def show(self):
        pass

    def hide(self):
        pass

    def destroy(self):
        pass


class Frame(object):
    """Base wrapper for a Tk widget whose geometry is managed explicitly using place()."""

    def __init__(self, parent, widget, callback=None):
        self.parent = parent
        self.widget = widget
        self.callback = callback
        self.x = 0
        self.y = 0
        self.width, self.height = self.natural_dimensions()
        self.shown = False
        self._tip = None

    def container(self):
        """Return the Tk widget in which children of this frame are created."""
        return self.widget

    def natural_dimensions(self):
        """Return the width and height the widget would like to have."""
        self.widget.update_idletasks()
        return max(self.widget.winfo_reqwidth(), 1), max(self.widget.winfo_reqheight(), 1)

    def geometry(self):
        """Return (x, y, width, height), with position relative to the parent frame."""
        return (self.x, self.y, self.width, self.height)

    def set_x(self, x):
        self.x = int(x)
        self._apply()

    def set_y(self, y):
        self.y = int(y)
        self._apply()

    def set_position(self, x, y):
        self.x = int(x)
        self.y = int(y)
        self._apply()

    def set_width(self, width):
        self.width = max(int(width), 1)
        self._apply()

    def set_height(self, height):
        self.height = max(int(height), 1)
        self._apply()

    def set_natural_size(self):
        self.width, self.height = self.natural_dimensions()
        self._apply()

    def show(self):
        self.shown = True
        self._apply()

    def hide(self):
        self.shown = False
        self.widget.place_forget()

    def _apply(self):
        if self.shown:
            self.widget.place(x=self.x, y=self.y, width=self.width, height=self.height, bordermode='outside')

    def get_state(self):
        return ''

    def set_state(self, state):
        pass

    def tooltip_widget(self):
        return self.widget

    def set_tooltip(self, text):
        if self._tip is None:
            self._tip = _ToolTip(self.tooltip_widget(), text)
        else:
            self._tip.text = text

    def notify(self, *args):
        """Send an event to the C++ frame which owns this wrapper."""
        if self.callback is not None:
            self.callback(*args)

    def destroy(self):
        self.callback = None
        if self._tip is not None:
            self._tip.hide()
        try:
            self.widget.destroy()
        except tk.TclError:
            pass


class MainFrame(Frame):
    """Top-level window on the desktop."""

    def __init__(self, parent, width, height, title, callback):
        top = tk.Toplevel(parent.container())
        top.withdraw()
        top.title(title)
        self._natural = (max(int(width), 1), max(int(height), 1))
        self._applied = None
        Frame.__init__(self, parent, top, callback)
        # Give window manager a hint about where to display the window.
        top.geometry('%dx%d+100+50' % (self.width, self.height))
        self._applied = (self.width, self.height)
        top.protocol('WM_DELETE_WINDOW', lambda: self.notify('close'))
        top.bind('<Configure>', self._configure)

    def natural_dimensions(self):
        return self._natural

    def geometry(self):
        return (0, 0, self.width, self.height)

    def set_x(self, x):
        pass

    def set_y(self, y):
        pass

    def set_position(self, x, y):
        pass

    def show(self):
        self.shown = True
        self._apply()
        self.widget.deiconify()

    def hide(self):
        self.shown = False
        self.widget.withdraw()

    def _apply(self):
        size = (self.width, self.height)
        if self._applied is not None and size != self._applied:
            self._applied = size
            self.widget.geometry('%dx%d' % size)

    def _configure(self, event):
        # Configure events for all children are also delivered to the toplevel; only handle its own.
        if event.widget is not self.widget or not self.shown:
            return
        size = (event.width, event.height)
        if size != (self.width, self.height):
            self.width, self.height = size
            self._applied = size
            self.notify('layout')


class Composite(Frame):
    """Plain container for other frames."""

    def __init__(self, parent, text, callback):
        Frame.__init__(self, parent, tk.Frame(parent.container(), width=20, height=20), callback)


class GroupFrame(Frame):
    """Container for other frames, drawn with a border and a title."""

    def __init__(self, parent, text, callback):
        Frame.__init__(self, parent, tk.LabelFrame(parent.container(), text=text), callback)


class Label(Frame):
    """Right-justified static text."""

    def __init__(self, parent, text, callback):
        Frame.__init__(self, parent, tk.Label(parent.container(), text=text, anchor='e'), callback)


class Button(Frame):
    """Push button with a text label."""

    def __init__(self, parent, text, callback):
        button = tk.Button(parent.container(), text=text, command=lambda: self.notify('clicked'))
        Frame.__init__(self, parent, button, callback)

    def get_state(self):
        return 'up'


class CheckButton(Frame):
    """Check box with a text label. State is "down" when checked, "up" otherwise."""

    def __init__(self, parent, text, callback):
        self.var = tk.IntVar(master=parent.container(), value=0)
        button = tk.Checkbutton(parent.container(), text=text, variable=self.var,
                                command=lambda: self.notify('clicked'))
        Frame.__init__(self, parent, button, callback)

    def get_state(self):
        return 'down' if self.var.get() else 'up'

    def set_state(self, state):
        if state == 'down':
            self.var.set(1)
        elif state == 'up':
            self.var.set(0)


class TextEntry(Frame):
    """Single line text entry. Its natural width is large enough to hold its initial contents."""

    def __init__(self, parent, text, callback):
        self.var = tk.StringVar(master=parent.container(), value=text)
        entry = tk.Entry(parent.container(), textvariable=self.var, width=max(len(text), 1) + 1)
        Frame.__init__(self, parent, entry, callback)
        self.var.trace_add('write', lambda *args: self.notify('modified', self.var.get()))

    def get_state(self):
        return self.var.get()

    def set_state(self, state):
        # Only change text if it is different, to prevent the cursor from jumping while the user types.
        if self.var.get() != state:
            self.var.set(state)


class TabFolder(Frame):
    """Tabbed folder widget holding any number of tabbed sheets (TabPage objects)."""

    def __init__(self, parent, callback):
        Frame.__init__(self, parent, ttk.Notebook(parent.container()), callback)

    def add_page(self, label, callback):
        return TabPage(self, label, callback)

    def selected(self):
        current = self.widget.select()
        if not current:
            return ''
        return self.widget.tab(current, 'text')

    def select_page(self, page):
        self.widget.select(page.widget)


class TabPage(Frame):
    """One sheet of a TabFolder. The notebook, not place(), manages where the sheet is shown."""

    def __init__(self, folder, label, callback):
        page = tk.Frame(folder.widget, width=1, height=1)
        folder.widget.add(page, text=label)
        Frame.__init__(self, folder, page, callback)

    def geometry(self):
        return (0, 0, self.width, self.height)

    def set_x(self, x):
        pass

    def set_y(self, y):
        pass

    def set_position(self, x, y):
        pass

    def show(self):
        self.shown = True
        self._apply()

    def hide(self):
        self.shown = False

    def _apply(self):
        self.widget.configure(width=self.width, height=self.height)


def _hist_polyline(x_low, x_high, y):
    """Convert histogram bins into a step line. Gaps between bins are drawn at 0."""
    xs = []
    ys = []
    n = len(x_low)
    for i in range(n):
        xs.extend((x_low[i], x_high[i]))
        ys.extend((y[i], y[i]))
        if i != n - 1 and x_low[i + 1] > x_high[i]:
            xs.extend((x_high[i], x_low[i + 1]))
            ys.extend((0., 0.))
    return xs, ys


def _smooth(x, y):
    """Return points of a smooth curve through (x, y), or the input points if this cannot be done."""
    import numpy as np
    try:
        from scipy.interpolate import make_interp_spline
    except ImportError:
        return x, y
    order = np.argsort(x)
    xs = np.asarray(x, dtype=float)[order]
    ys = np.asarray(y, dtype=float)[order]
    if len(xs) < 3 or np.any(np.diff(xs) <= 0.):
        return x, y
    xx = np.linspace(xs[0], xs[-1], max(10 * len(xs), 200))
    return xx, make_interp_spline(xs, ys, k=min(3, len(xs) - 1))(xx)


def _log_axis(values):
    """Convert positive values to log10 for 3d axes, which matplotlib cannot draw logarithmically."""
    import numpy as np
    values = np.asarray(values, dtype=float)
    if values.size == 0 or np.any(values <= 0.):
        return values, False
    return np.log10(values), True


def _power_formatter(offset=0.):
    from matplotlib.ticker import FuncFormatter
    return FuncFormatter(lambda value, pos: '$10^{%g}$' % (value + offset))


def _lego_mesh(x_edges, y_edges, z):
    """Build X, Y, Z grids which make plot_surface draw a lego (3d bar) histogram."""
    import numpy as np
    x_edges = np.asarray(x_edges, dtype=float)
    y_edges = np.asarray(y_edges, dtype=float)
    z = np.asarray(z, dtype=float)
    nx = len(x_edges) - 1
    ny = len(y_edges) - 1
    # Each edge appears twice, so that neighboring bins are joined by vertical walls.
    xs = np.repeat(x_edges, 2)
    ys = np.repeat(y_edges, 2)
    # Grid points 1 .. 2n belong to bin (k - 1) // 2; the outermost points form the floor.
    x_bin = (np.arange(1, 2 * nx + 1) - 1) // 2
    y_bin = (np.arange(1, 2 * ny + 1) - 1) // 2
    zz = np.zeros((len(ys), len(xs)))
    zz[1:2 * ny + 1, 1:2 * nx + 1] = z[np.ix_(x_bin, y_bin)].T
    xx, yy = np.meshgrid(xs, ys)
    return xx, yy, zz


class PlotFrame(Frame):
    """Frame holding a matplotlib figure and its navigation toolbar."""

    def __init__(self, parent, width, height, callback):
        from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
        from matplotlib.figure import Figure
        self._natural = (max(int(width), 1), max(int(height), 1))
        frame = tk.Frame(parent.container(), width=self._natural[0], height=self._natural[1])
        Frame.__init__(self, parent, frame, callback)
        frame.pack_propagate(False)

        self.figure = Figure(figsize=(self.width / 100., self.height / 100.), dpi=100, facecolor='w', edgecolor='w')
        self.canvas = FigureCanvasTkAgg(self.figure, master=frame)
        self.toolbar = NavigationToolbar2Tk(self.canvas, frame, pack_toolbar=False)
        self.toolbar.update()
        self.toolbar.pack(side=tk.BOTTOM, fill=tk.X)
        self.canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=True)

        self.axes = None
        self.dimensionality = 0
        self.markers = []
        self._home_xlim = None
        self._drag = None
        self._span = None
        self.canvas.mpl_connect('button_press_event', self._press)
        self.canvas.mpl_connect('motion_notify_event', self._motion)
        self.canvas.mpl_connect('button_release_event', self._release)

    def natural_dimensions(self):
        return self._natural

    def tooltip_widget(self):
        return self.canvas.get_tk_widget()

    def destroy(self):
        self.clear()
        Frame.destroy(self)

    # Plotting.
    def clear(self):
        self.figure.clear()
        self.axes = None
        self.dimensionality = 0
        self.markers = []
        self._drag = None
        self._span = None

    def begin(self, dimensionality):
        """Create the axes for a plot with the given number of dimensions (2 or 3)."""
        self.clear()
        self.dimensionality = dimensionality
        if dimensionality == 3:
            self.axes = self.figure.add_subplot(111, projection='3d')
            self.figure.subplots_adjust(left=0.05, bottom=0.05, right=0.95, top=0.95)
        else:
            self.axes = self.figure.add_subplot(111)
            self.figure.subplots_adjust(left=0.125, bottom=0.15, right=0.9, top=0.9)

    def plot_hist(self, x_low, x_high, y, color, line_style):
        xs, ys = _hist_polyline(x_low, x_high, y)
        style = _LINE_STYLES.get(line_style, '-')
        if style == 'none':
            self.axes.plot(xs, ys, color=color, linestyle='none', marker='.', markersize=3)
        else:
            self.axes.plot(xs, ys, color=color, linestyle=style, linewidth=1.0)

    def plot_scatter(self, x, x_low, x_high, y, y_low, y_high, color, line_style, curve_type):
        x_err = [x_low, x_high] if any(x_low) or any(x_high) else None
        y_err = [y_low, y_high] if any(y_low) or any(y_high) else None
        style = _LINE_STYLES.get(line_style, '-')
        if style == 'none':
            self.axes.errorbar(x, y, yerr=y_err, xerr=x_err, color=color, linestyle='none', marker='.',
                               markersize=3, elinewidth=1.0)
        elif curve_type == 'curve':
            self.axes.errorbar(x, y, yerr=y_err, xerr=x_err, color=color, linestyle='none', elinewidth=1.0)
            xs, ys = _smooth(x, y)
            self.axes.plot(xs, ys, color=color, linestyle=style, linewidth=1.0)
        else:
            self.axes.errorbar(x, y, yerr=y_err, xerr=x_err, color=color, linestyle=style, linewidth=1.0,
                               elinewidth=1.0)

    def plot_lego(self, x_edges, y_edges, z, x_log, y_log, z_log):
        import numpy as np
        if x_log:
            x_edges, x_log = _log_axis(x_edges)
        if y_log:
            y_edges, y_log = _log_axis(y_edges)
        xx, yy, zz = _lego_mesh(x_edges, y_edges, z)
        z_offset = 0.
        if z_log:
            positive = zz[zz > 0.]
            if positive.size:
                z_offset = np.floor(np.log10(positive.min()))
                zz = np.where(zz > 0., np.log10(np.where(zz > 0., zz, 1.)) - z_offset, 0.)
            else:
                z_log = False
        self.axes.plot_surface(xx, yy, zz, rstride=1, cstride=1, color='w', edgecolor='k', linewidth=0.3,
                               shade=True)
        if x_log:
            self.axes.xaxis.set_major_formatter(_power_formatter())
        if y_log:
            self.axes.yaxis.set_major_formatter(_power_formatter())
        if z_log:
            self.axes.zaxis.set_major_formatter(_power_formatter(z_offset))

    def finish(self, title, x_label, y_label, z_label, x_log, y_log):
        """Set titles and scales once all plots have been drawn."""
        if self.axes is None:
            return
        self.axes.set_title(title)
        if x_label:
            self.axes.set_xlabel(x_label)
        if y_label:
            self.axes.set_ylabel(y_label)
        if self.dimensionality == 3:
            if z_label:
                self.axes.set_zlabel(z_label)
            return
        self.axes.set_xscale('log' if x_log else 'linear')
        self.axes.set_yscale('log' if y_log else 'linear')
        # Do not call relim() here: it ignores collections, and so would exclude error bars from the range.
        self.axes.autoscale_view()
        # Freeze the limits so that markers do not change the range of the plot.
        self.axes.set_autoscale_on(False)
        self._home_xlim = self.axes.get_xlim()

    def add_marker(self, plot_index, marker_index, x, y, text, color):
        if self.axes is None or self.dimensionality != 2:
            return
        color = color_name(color)
        point, = self.axes.plot([x], [y], linestyle='none', marker='v', markersize=8, color=color)
        label = self.axes.text(x, y, ' ' + text, rotation=45, rotation_mode='anchor', color=color,
                               horizontalalignment='left', verticalalignment='bottom', clip_on=True)
        self.markers.append({'plot': plot_index, 'marker': marker_index, 'point': point, 'label': label})

    def draw(self):
        global _snapshot_count
        self.canvas.draw_idle()
        directory = os.environ.get('ST_GRAPH_SNAPSHOT_DIR')
        if directory:
            _snapshot_count += 1
            self.figure.savefig(os.path.join(directory, 'st_graph_%03d.png' % _snapshot_count))

    # Mouse interaction for 2d plots: drag markers, left-drag to zoom the X axis, right-click to report.
    def _interactive(self, event):
        return (self.dimensionality == 2 and self.axes is not None and event.inaxes is self.axes and
                not self.toolbar.mode)

    def _find_marker(self, event):
        for marker in reversed(self.markers):
            if marker['point'].contains(event)[0] or marker['label'].contains(event)[0]:
                return marker
        return None

    def _press(self, event):
        if event.button != 1 or not self._interactive(event):
            return
        self._drag = self._find_marker(event)
        if self._drag is not None:
            return
        if event.dblclick:
            # Double click restores the original X range.
            if self._home_xlim is not None:
                self.axes.set_xlim(self._home_xlim)
                self.canvas.draw_idle()
            return
        band = self.axes.axvspan(event.xdata, event.xdata, color='0.8', alpha=0.5)
        self._span = {'x0': event.xdata, 'px0': event.x, 'band': band}

    def _motion(self, event):
        if event.inaxes is not self.axes or event.xdata is None:
            return
        if self._drag is not None:
            self._drag['point'].set_data([event.xdata], [event.ydata])
            self._drag['label'].set_position((event.xdata, event.ydata))
            self.canvas.draw_idle()
        elif self._span is not None:
            low = min(self._span['x0'], event.xdata)
            high = max(self._span['x0'], event.xdata)
            self._span['band'].remove()
            self._span['band'] = self.axes.axvspan(low, high, color='0.8', alpha=0.5)
            self.canvas.draw_idle()

    def _release(self, event):
        if self._drag is not None:
            marker = self._drag
            self._drag = None
            x, y = marker['point'].get_data()
            self.notify('markerMoved', marker['plot'], marker['marker'], float(x[0]), float(y[0]))
            return
        if self._span is not None:
            span = self._span
            self._span = None
            span['band'].remove()
            if event.xdata is not None and abs(event.x - span['px0']) > 5:
                low = min(span['x0'], event.xdata)
                high = max(span['x0'], event.xdata)
                self.axes.set_xlim(low, high)
            self.canvas.draw_idle()
            return
        if event.button == 3 and self._interactive(event):
            self.notify('rightClicked', float(event.xdata), float(event.ydata))
