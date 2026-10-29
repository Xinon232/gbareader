#!/usr/bin/env python3
"""gbcomic converter with a window: comics (PDF, EPUB, CBZ, picture folders)
to .gbcomic files for gbareader. The work is done by gbcomic.py (next to
this file); this is only the window around it.

    python3 gbcomic_gui.py [comic ...]
"""
import os
import queue
import sys
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from PIL import Image, ImageTk

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gbcomic  # noqa: E402
import gbcomic_preview as preview  # noqa: E402

TITLE = 'gbcomic converter'
COMIC_TYPES = [('Comics', '*.pdf *.epub *.cbz *.zip *.jpg *.jpeg *.png *.webp *.gif *.bmp *.tif *.tiff'),
               ('All files', '*.*')]


class Stop(Exception):
    """Cancel pressed: the comic being converted is left unfinished (and removed)."""


class Window:
    def __init__(self, root, files):
        self.root = root
        root.title(TITLE)
        root.minsize(620, 560)
        self.events = queue.Queue()
        self.worker = None
        self.cancel = threading.Event()

        outer = ttk.Frame(root, padding=12)
        outer.pack(fill='both', expand=True)
        outer.columnconfigure(0, weight=1)

        # Comics to convert.
        box = ttk.LabelFrame(outer, text='Comics (PDF, EPUB, CBZ, picture folders)', padding=8)
        box.grid(row=0, column=0, sticky='nsew')
        outer.rowconfigure(0, weight=1)
        box.columnconfigure(0, weight=1)
        box.rowconfigure(0, weight=1)
        # The list shows each comic's name, then its folder; self.paths has the full paths.
        self.paths = []
        self.files = tk.Listbox(box, height=6, selectmode='extended', activestyle='none')
        self.files.grid(row=0, column=0, sticky='nsew')
        scroll = ttk.Scrollbar(box, orient='vertical', command=self.files.yview)
        scroll.grid(row=0, column=1, sticky='ns')
        self.files.configure(yscrollcommand=scroll.set)
        buttons = ttk.Frame(box)
        buttons.grid(row=0, column=2, sticky='n', padx=(8, 0))
        for text, command in (('Add comics…', self.add_files), ('Add folder…', self.add_folder),
                              ('Remove', self.remove), ('Clear', self.clear)):
            ttk.Button(buttons, text=text, command=command, width=14).pack(fill='x', pady=2)
        ttk.Label(box, text='A folder of pictures is one comic. Several comics get the same settings.',
                  foreground='#666').grid(row=1, column=0, columnspan=3, sticky='w', pady=(6, 0))

        # Angles and zooms.
        screens = ttk.LabelFrame(outer, text='Screens', padding=8)
        screens.grid(row=1, column=0, sticky='ew', pady=(10, 0))
        screens.columnconfigure(1, weight=1)
        ttk.Label(screens, text='Angles:').grid(row=0, column=0, sticky='w')
        self.angles = tk.StringVar(value='both')
        row = ttk.Frame(screens)
        row.grid(row=0, column=1, sticky='w')
        for text, value in (('Both', 'both'), ('Upright only (Normal, Upside down)', 'upright'),
                            ('Sideways only (90°)', 'sideways')):
            ttk.Radiobutton(row, text=text, value=value, variable=self.angles,
                            command=self.update).pack(side='left', padx=(0, 12))
        self.zoom_vars = {0: {}, 1: {}}
        self.zoom_boxes = {0: {}, 1: {}}
        self.zoom_rows = {}
        defaults = {0: gbcomic.factors(gbcomic.DEFAULT_NORMAL, gbcomic.NORMAL_CHOICES, 'normal'),
                    1: gbcomic.factors(gbcomic.DEFAULT_SIDEWAYS, gbcomic.SIDEWAYS_CHOICES, 'sideways')}
        for orientation, label, choices in ((0, 'Upright zooms:', gbcomic.NORMAL_CHOICES),
                                            (1, 'Sideways zooms:', gbcomic.SIDEWAYS_CHOICES)):
            ttk.Label(screens, text=label).grid(row=1 + orientation, column=0, sticky='w', pady=(6, 0))
            row = ttk.Frame(screens)
            row.grid(row=1 + orientation, column=1, sticky='w', pady=(6, 0))
            self.zoom_rows[orientation] = row
            for i, value in enumerate(choices):
                var = tk.BooleanVar(value=value in defaults[orientation])
                box_ = ttk.Checkbutton(row, text=gbcomic.zoom_text(value), variable=var,
                                       command=self.update)
                box_.grid(row=i // 10, column=i % 10, sticky='w')
                self.zoom_vars[orientation][value] = var
                self.zoom_boxes[orientation][value] = box_
        ttk.Label(screens, text=f'Fit page and Fit width are always included. Default: '
                                f'{gbcomic.DEFAULT_NORMAL.replace(",", ", ")} and '
                                f'{gbcomic.DEFAULT_SIDEWAYS.replace(",", ", ")}. Preview: see a zoom on the GBA screen.',
                  foreground='#666').grid(row=3, column=0, columnspan=2, sticky='w', pady=(6, 0))
        side = ttk.Frame(screens)
        side.grid(row=0, column=2, rowspan=3, sticky='ne')
        ttk.Button(side, text='Defaults', command=lambda: self.set_defaults(defaults)).pack(fill='x')
        self.preview_button = ttk.Button(side, text='Preview…', command=self.open_preview)
        self.preview_button.pack(fill='x', pady=(4, 0))
        self.preview_window = None

        # Picture options.
        more = ttk.LabelFrame(outer, text='Pictures', padding=8)
        more.grid(row=2, column=0, sticky='ew', pady=(10, 0))
        self.sharpen = tk.StringVar(value='light')
        self.margins = tk.StringVar(value='light')
        self.spreads = tk.StringVar(value='auto')
        self.dpi = tk.IntVar(value=200)
        self.dither = tk.BooleanVar(value=False)
        self.boost = tk.BooleanVar(value=False)
        for column, (label, var, values) in enumerate((('Sharpen', self.sharpen, ('off', 'light', 'strong')),
                                                        ('Crop margins', self.margins, ('off', 'light', 'strong')),
                                                        ('Spreads in halves', self.spreads, ('auto', 'always', 'never')))):
            ttk.Label(more, text=label + ':').grid(row=0, column=2 * column, sticky='w', padx=(0, 4))
            ttk.Combobox(more, textvariable=var, values=values, state='readonly', width=7).grid(
                row=0, column=2 * column + 1, sticky='w', padx=(0, 14))
        ttk.Label(more, text='PDF resolution (dpi):').grid(row=1, column=0, columnspan=2, sticky='w', pady=(6, 0))
        ttk.Spinbox(more, from_=100, to=400, increment=25, textvariable=self.dpi, width=6).grid(
            row=1, column=2, sticky='w', pady=(6, 0))
        ttk.Checkbutton(more, text='Dither colours', variable=self.dither).grid(row=1, column=3, columnspan=2,
                                                                                sticky='w', pady=(6, 0))
        ttk.Checkbutton(more, text='Keep small lettering dark', variable=self.boost).grid(
            row=1, column=5, sticky='w', pady=(6, 0))

        # Output.
        out = ttk.LabelFrame(outer, text='Save', padding=8)
        out.grid(row=3, column=0, sticky='ew', pady=(10, 0))
        out.columnconfigure(1, weight=1)
        self.next_to = tk.BooleanVar(value=True)
        self.folder = tk.StringVar(value='')
        ttk.Radiobutton(out, text='Next to each comic', value=True, variable=self.next_to,
                        command=self.update).grid(row=0, column=0, sticky='w')
        row = ttk.Frame(out)
        row.grid(row=1, column=0, columnspan=3, sticky='ew', pady=(4, 0))
        row.columnconfigure(1, weight=1)
        ttk.Radiobutton(row, text='In this folder:', value=False, variable=self.next_to,
                        command=self.update).grid(row=0, column=0, sticky='w')
        ttk.Entry(row, textvariable=self.folder).grid(row=0, column=1, sticky='ew', padx=6)
        ttk.Button(row, text='Choose…', command=self.choose_folder).grid(row=0, column=2)

        # Size, progress, buttons.
        bottom = ttk.Frame(outer)
        bottom.grid(row=4, column=0, sticky='ew', pady=(10, 0))
        bottom.columnconfigure(0, weight=1)
        self.estimate = ttk.Label(bottom, text='')
        self.estimate.grid(row=0, column=0, sticky='w')
        self.convert_button = ttk.Button(bottom, text='Convert', command=self.start)
        self.convert_button.grid(row=0, column=1, sticky='e')
        self.cancel_button = ttk.Button(bottom, text='Cancel', command=self.stop, state='disabled')
        self.cancel_button.grid(row=0, column=2, sticky='e', padx=(6, 0))
        self.progress = tk.DoubleVar(value=0)
        self.bar = ttk.Progressbar(bottom, mode='determinate', variable=self.progress, maximum=1)
        self.bar.grid(row=1, column=0, columnspan=3, sticky='ew', pady=(8, 0))
        self.status = ttk.Label(bottom, text='Add comics, then press Convert.')
        self.status.grid(row=2, column=0, columnspan=3, sticky='w', pady=(4, 0))
        self.log = tk.Text(outer, height=6, wrap='word', state='disabled', relief='flat',
                           background=root.cget('background'), font='TkDefaultFont')
        self.log.grid(row=5, column=0, sticky='nsew', pady=(8, 0))
        outer.rowconfigure(5, weight=1)

        for f in files:
            self.add(f)
        self.update()
        root.after(100, self.poll)

    # ---------------------------------------------------------------- files
    def add(self, path):
        path = os.path.abspath(path)
        if path not in self.paths:
            self.paths.append(path)
            folder, name = os.path.split(path.rstrip('/\\'))
            self.files.insert('end', f'{name}    ({folder})')

    def add_files(self):
        for f in filedialog.askopenfilenames(title='Add comics', filetypes=COMIC_TYPES):
            self.add(f)
        self.update()

    def add_folder(self):
        folder = filedialog.askdirectory(title='Add a folder of pictures (one comic)')
        if folder:
            self.add(folder)
        self.update()

    def remove(self):
        for i in reversed(self.files.curselection()):
            self.files.delete(i)
            del self.paths[i]
        self.update()

    def clear(self):
        self.files.delete(0, 'end')
        self.paths = []
        self.update()

    def choose_folder(self):
        folder = filedialog.askdirectory(title='Save the .gbcomic files in')
        if folder:
            self.folder.set(folder)
            self.next_to.set(False)
        self.update()

    # ---------------------------------------------------------------- settings
    def chosen(self, orientation):
        return sorted(v for v, var in self.zoom_vars[orientation].items() if var.get())

    def set_defaults(self, defaults):
        for orientation in (0, 1):
            for value, var in self.zoom_vars[orientation].items():
                var.set(value in defaults[orientation])
        self.update()

    def update(self):
        angles = self.angles.get()
        used = {0: angles in ('both', 'upright'), 1: angles in ('both', 'sideways')}
        for orientation in (0, 1):
            for value, box_ in self.zoom_boxes[orientation].items():
                box_.state(['!disabled'] if used[orientation] else ['disabled'])
        kb = sum(gbcomic.estimate_kb(o, self.chosen(o)) for o in (0, 1) if used[o])
        self.estimate.configure(text=f'About {kb / 1024:.1f} MB per page for a colour comic')
        busy = self.worker is not None
        self.convert_button.state(['disabled'] if busy or not self.files.size() else ['!disabled'])
        self.preview_button.state(['disabled'] if not self.files.size() else ['!disabled'])
        if self.preview_window is not None:
            self.preview_window.comics_changed()
        self.cancel_button.state(['!disabled'] if busy else ['disabled'])

    def write(self, text):
        self.log.configure(state='normal')
        self.log.insert('end', text + '\n')
        self.log.see('end')
        self.log.configure(state='disabled')

    def picture_options(self):
        try:
            dpi = int(self.dpi.get())
        except (tk.TclError, ValueError):
            dpi = 200
        return Options(sharpen=self.sharpen.get(), margins=self.margins.get(), spreads=self.spreads.get(),
                       dpi=max(50, min(600, dpi)), dither=self.dither.get(), boost_lettering=self.boost.get())

    def open_preview(self):
        if not self.paths:
            return
        if self.preview_window is None:
            self.preview_window = PreviewWindow(self)
        else:
            self.preview_window.top.lift()
        selected = self.files.curselection()
        self.preview_window.show_comic(selected[0] if selected else 0)

    # ---------------------------------------------------------------- converting
    def start(self):
        sources = list(self.paths)
        if not sources:
            return
        angles = self.angles.get()
        normal, sideways = self.chosen(0), self.chosen(1)
        folder = None if self.next_to.get() else self.folder.get().strip()
        if folder is not None and not os.path.isdir(folder):
            messagebox.showerror(TITLE, 'Choose a folder to save in (or save next to each comic).')
            return
        try:
            dpi = int(self.dpi.get())
        except (tk.TclError, ValueError):
            dpi = 200
        options = Options(angles=angles, sharpen=self.sharpen.get(), margins=self.margins.get(),
                          spreads=self.spreads.get(), dpi=max(50, min(600, dpi)), dither=self.dither.get(),
                          boost_lettering=self.boost.get(), verify=False,
                          zoom_list=gbcomic.zoom_list(angles, normal, sideways))
        self.cancel.clear()
        self.progress.set(0)
        self.worker = threading.Thread(target=self.run, args=(sources, folder, options), daemon=True)
        self.worker.start()
        self.write(f'Converting {len(sources)} comic{"s" if len(sources) > 1 else ""}…')
        self.update()

    def stop(self):
        self.cancel.set()
        self.status.configure(text='Stopping after this page…')

    def run(self, sources, folder, options):
        """Worker thread: converts each comic; tells the window through self.events."""
        counts = [gbcomic.count_pages(s) for s in sources]
        total = sum(counts) or len(sources)
        done = 0
        for index, source in enumerate(sources):
            name = os.path.basename(source.rstrip('/\\')) or source
            base = os.path.splitext(name)[0] if not os.path.isdir(source) else name
            output = os.path.join(folder or os.path.dirname(source.rstrip('/\\')), base + '.gbcomic')
            options.source = source

            def progress(count, page, index=index, name=name):
                if self.cancel.is_set():
                    raise Stop()
                self.events.put(('progress', done + count, total,
                                 f'{name} ({index + 1} of {len(sources)}): page {count}'
                                 + (f' of {counts[index]}' if counts[index] else '')))
            options.progress = progress
            try:
                pages, _ = gbcomic.convert(options, output)
                size = os.path.getsize(output)
                self.events.put(('log', f'{os.path.basename(output)}: {pages} pages, {size / 2**20:.1f} MB'))
                done += counts[index] or pages
            except Stop:
                if os.path.exists(output + '.part'):
                    os.remove(output + '.part')
                self.events.put(('log', f'{name}: stopped'))
                break
            except (SystemExit, Exception) as e:
                if os.path.exists(output + '.part'):
                    os.remove(output + '.part')
                self.events.put(('log', f'{name}: not converted ({e})'))
                done += counts[index]
        self.events.put(('done', None))

    def poll(self):
        try:
            while True:
                event = self.events.get_nowait()
                if event[0] == 'progress':
                    _, value, total, text = event
                    self.bar.configure(maximum=total)
                    self.progress.set(value)
                    self.status.configure(text=text)
                elif event[0] == 'log':
                    self.write(event[1])
                elif event[0] == 'done':
                    self.worker = None
                    self.status.configure(text='Done. Copy the .gbcomic files to /gbareader on the SD card.'
                                          if not self.cancel.is_set() else 'Stopped.')
                    if not self.cancel.is_set():
                        self.bar.configure(maximum=1)
                        self.progress.set(1)
                    self.update()
        except queue.Empty:
            pass
        self.root.after(100, self.poll)


class PreviewWindow:
    """One page at one zoom on the GBA screen (240 x 160 pixels; 160 x 240
    sideways), made as the converter will make it and read as the reader does.
    Mouse: drag moves the view (the D-pad), click right half A, left half B,
    wheel scrolls. Keys: arrows, X / Enter (A), Z / Backspace (B), Page Up / Down."""

    STEP = 16   # the D-pad, as Static scrolling on the GBA

    def __init__(self, main):
        self.main = main
        self.top = tk.Toplevel(main.root)
        self.top.title('Preview - ' + TITLE)
        self.top.protocol('WM_DELETE_WINDOW', self.close)
        self.events = queue.Queue()
        self.cache = {}          # (comic, page, angle, zoom, options) -> (pictures, page size)
        self.source = None       # PageSource of the comic shown
        self.source_path = None
        self.pages = 0
        self.page = 0
        self.view = None
        self.want = None         # what is being prepared
        self.busy = False
        self.drag = None
        self.photo = None

        bar = ttk.Frame(self.top, padding=(10, 8, 10, 0))
        bar.pack(fill='x')
        ttk.Label(bar, text='Comic:').grid(row=0, column=0, sticky='w')
        self.comic = ttk.Combobox(bar, state='readonly', width=40)
        self.comic.grid(row=0, column=1, columnspan=5, sticky='ew', padx=(4, 0))
        self.comic.bind('<<ComboboxSelected>>', lambda e: self.show_comic(self.comic.current()))
        self.angle = tk.IntVar(value=0)
        ttk.Label(bar, text='Angle:').grid(row=1, column=0, sticky='w', pady=(6, 0))
        angles = ttk.Frame(bar)
        angles.grid(row=1, column=1, sticky='w', pady=(6, 0), padx=(4, 0))
        for text, value in (('Upright', 0), ('Sideways (90°)', 1)):
            ttk.Radiobutton(angles, text=text, value=value, variable=self.angle,
                            command=self.angle_changed).pack(side='left', padx=(0, 10))
        ttk.Label(bar, text='Zoom:').grid(row=1, column=2, sticky='e', pady=(6, 0), padx=(10, 4))
        self.zoom = ttk.Combobox(bar, state='readonly', width=10)
        self.zoom.grid(row=1, column=3, sticky='w', pady=(6, 0))
        self.zoom.bind('<<ComboboxSelected>>', lambda e: self.prepare())
        ttk.Button(bar, text='-', width=3, command=lambda: self.zoom_step(-1)).grid(row=1, column=4, pady=(6, 0))
        ttk.Button(bar, text='+', width=3, command=lambda: self.zoom_step(1)).grid(row=1, column=5, pady=(6, 0))
        ttk.Label(bar, text='Reading:').grid(row=2, column=0, sticky='w', pady=(6, 0))
        self.order = ttk.Combobox(bar, state='readonly', width=14, values=preview.ORDER_NAMES)
        self.order.current(0)
        self.order.grid(row=2, column=1, sticky='w', pady=(6, 0), padx=(4, 0))
        self.order.bind('<<ComboboxSelected>>', lambda e: self.prepare())
        self.skip_empty = tk.BooleanVar(value=True)
        self.page_map = tk.BooleanVar(value=True)
        checks = ttk.Frame(bar)
        checks.grid(row=2, column=2, columnspan=4, sticky='w', pady=(6, 0), padx=(10, 0))
        ttk.Checkbutton(checks, text='Skip empty', variable=self.skip_empty, command=self.prepare).pack(side='left')
        ttk.Checkbutton(checks, text='Page map', variable=self.page_map, command=self.draw).pack(side='left', padx=(10, 0))
        ttk.Label(checks, text='Size:').pack(side='left', padx=(14, 4))
        self.scale = tk.IntVar(value=2)
        for n in (1, 2, 3, 4):
            ttk.Radiobutton(checks, text=f'{n}×', value=n, variable=self.scale, command=self.draw).pack(side='left')

        # The screen.
        middle = ttk.Frame(self.top, padding=10)
        middle.pack(fill='both', expand=True)
        self.canvas = tk.Canvas(middle, width=240 * 2, height=160 * 2, background='#222', highlightthickness=0,
                                cursor='fleur')
        self.canvas.pack(expand=True)
        self.canvas.bind('<ButtonPress-1>', self.press)
        self.canvas.bind('<B1-Motion>', self.motion)
        self.canvas.bind('<ButtonRelease-1>', self.release)
        self.canvas.bind('<MouseWheel>', self.wheel)
        self.canvas.bind('<Shift-MouseWheel>', lambda e: self.wheel(e, sideways=True))
        self.canvas.bind('<Button-4>', lambda e: self.move(0, -self.STEP))
        self.canvas.bind('<Button-5>', lambda e: self.move(0, self.STEP))

        # Buttons: pages, screens, the D-pad.
        controls = ttk.Frame(self.top, padding=(10, 0, 10, 10))
        controls.pack(fill='x')
        pages = ttk.Frame(controls)
        pages.pack(side='left')
        for text, command in (('|< First', lambda: self.go_page(0)), ('< Page', lambda: self.go_page(self.page - 1))):
            ttk.Button(pages, text=text, width=7, command=command).pack(side='left')
        self.page_text = tk.StringVar(value='1')
        entry = ttk.Entry(pages, textvariable=self.page_text, width=6, justify='center')
        entry.pack(side='left', padx=4)
        entry.bind('<Return>', lambda e: self.go_page_typed())
        self.page_count = ttk.Label(pages, text='of 0')
        self.page_count.pack(side='left', padx=(0, 4))
        for text, command in (('Page >', lambda: self.go_page(self.page + 1)),
                              ('Last >|', lambda: self.go_page(self.pages - 1))):
            ttk.Button(pages, text=text, width=7, command=command).pack(side='left')
        pad = ttk.Frame(controls)
        pad.pack(side='right')
        ab = ttk.Frame(pad)
        ab.grid(row=0, column=4, rowspan=3, padx=(14, 0))
        ttk.Button(ab, text='B   < screen', command=lambda: self.step(-1)).pack(fill='x')
        ttk.Button(ab, text='A   screen >', command=lambda: self.step(1)).pack(fill='x', pady=(4, 0))
        for text, row, column, dx, dy in (('Up', 0, 1, 0, -1), ('Left', 1, 0, -1, 0), ('Right', 1, 2, 1, 0), ('Down', 2, 1, 0, 1)):
            tk.Button(pad, text=text, width=5, repeatdelay=300, repeatinterval=60,
                      command=lambda dx=dx, dy=dy: self.move(dx * self.STEP, dy * self.STEP)).grid(row=row, column=column)
        self.status = ttk.Label(self.top, text='', padding=(10, 0, 10, 10), foreground='#444')
        self.status.pack(fill='x')

        for key, action in (('<Left>', lambda e: self.move(-self.STEP, 0)), ('<Right>', lambda e: self.move(self.STEP, 0)),
                            ('<Up>', lambda e: self.move(0, -self.STEP)), ('<Down>', lambda e: self.move(0, self.STEP)),
                            ('<x>', lambda e: self.step(1)), ('<Return>', lambda e: self.step(1)),
                            ('<z>', lambda e: self.step(-1)), ('<BackSpace>', lambda e: self.step(-1)),
                            ('<Prior>', lambda e: self.go_page(self.page - 1)),
                            ('<Next>', lambda e: self.go_page(self.page + 1))):
            self.top.bind(key, action)
        entry.bind('<Left>', lambda e: None)
        entry.bind('<Right>', lambda e: None)
        self.comics_changed()
        self.top.after(50, self.poll)

    # ---------------------------------------------------------------- choices
    def comics_changed(self):
        names = [os.path.basename(p.rstrip('/\\')) or p for p in self.main.paths]
        self.comic.configure(values=names)
        if self.source_path not in self.main.paths:
            if self.main.paths:
                self.show_comic(0)
            else:
                self.close()
        else:
            self.comic.current(self.main.paths.index(self.source_path))
        self.zoom_choices()

    def zoom_values(self):
        """(label, kind, factor) for the angle shown: Fit page, Fit width, every zoom."""
        values = [('Fit page', gbcomic.KIND_FIT_PAGE, 0), ('Fit width', gbcomic.KIND_WIDTH, 100)]
        choices = gbcomic.NORMAL_CHOICES if self.angle.get() == 0 else gbcomic.SIDEWAYS_CHOICES
        ticked = set(self.main.chosen(self.angle.get()))
        for v in choices:
            values.append((f'{v / 100:.1f}x' + (' (ticked)' if v in ticked else ''), gbcomic.KIND_WIDTH, v))
        return values

    def zoom_choices(self, keep=True):
        values = self.zoom_values()
        old = self.zoom.get().split()[0] if keep and self.zoom.get() else None
        self.zoom.configure(values=[v[0] for v in values])
        labels = [v[0].split()[0] for v in values]
        if old in labels:
            self.zoom.current(labels.index(old))
        else:
            ticked = self.main.chosen(self.angle.get())
            first = f'{ticked[0] / 100:.1f}x' if ticked else 'Fit width'
            self.zoom.current(labels.index(first))

    def angle_changed(self):
        self.zoom_choices(keep=False)
        self.prepare()

    def zoom_step(self, delta):
        i = max(0, min(len(self.zoom['values']) - 1, self.zoom.current() + delta))
        self.zoom.current(i)
        self.prepare()

    def show_comic(self, index):
        if not self.main.paths:
            return
        path = self.main.paths[index]
        self.comic.current(index)
        if path != self.source_path:
            # (Not while a page of it is being made in the background.)
            if self.source is not None and not self.busy:
                self.source.close()
            self.source_path = path
            options = self.main.picture_options()
            try:
                self.source = preview.PageSource(path, options.dpi)
                self.pages = self.source.count()
            except Exception as e:
                self.source, self.pages = None, 0
                self.status.configure(text=f'Cannot read this comic: {e}')
            self.page = 0
            self.page_count.configure(text=f'of {self.pages}')
        self.prepare()

    def go_page(self, page, at_end=False):
        if not self.pages:
            return
        page = max(0, min(self.pages - 1, page))
        if page != self.page or self.view is None:
            self.page = page
            self.prepare(at_end=at_end)

    def go_page_typed(self):
        try:
            self.go_page(int(self.page_text.get()) - 1)
        except ValueError:
            self.page_text.set(str(self.page + 1))
        self.top.focus_set()

    # ---------------------------------------------------------------- the page
    def prepare(self, at_end=False):
        """Makes the page at the zoom chosen (in the background), then shows it."""
        if self.source is None or not self.pages:
            return
        self.page_text.set(str(self.page + 1))
        _, kind, factor = self.zoom_values()[max(0, self.zoom.current())]
        options = self.main.picture_options()
        key = (self.source_path, self.page, self.angle.get(), kind, factor,
               options.sharpen, options.margins, options.spreads, options.dither, options.boost_lettering, options.dpi)
        self.want = (key, at_end)
        if key in self.cache:
            self.make_view(key, at_end)
            return
        self.status.configure(text=f'Preparing page {self.page + 1}…')
        if not self.busy:
            self.busy = True
            threading.Thread(target=self.work, args=(key, options), daemon=True).start()

    def work(self, key, options):
        """Worker thread: the page's pictures, made by the converter's own code."""
        try:
            path, page, angle, kind, factor = key[:5]
            source = self.source if path == self.source_path else preview.PageSource(path, options.dpi)
            im = source.page(page)
            pictures = preview.view_pictures(im, angle, kind, factor, options)
            self.events.put(('ready', key, (pictures, im.size)))
        except Exception as e:
            self.events.put(('failed', key, str(e)))

    def poll(self):
        try:
            while True:
                kind, key, value = self.events.get_nowait()
                self.busy = False
                if kind == 'ready':
                    if len(self.cache) > 24:
                        self.cache.clear()
                    self.cache[key] = value
                    if self.want and self.want[0] == key:
                        self.make_view(key, self.want[1])
                else:
                    self.status.configure(text=f'Cannot show this page: {value}')
                if self.want and self.want[0] not in self.cache and kind == 'ready':
                    self.prepare(self.want[1])
        except queue.Empty:
            pass
        if self.main.preview_window is self:   # (closed: stop asking)
            self.top.after(50, self.poll)

    def make_view(self, key, at_end):
        pictures, size = self.cache[key]
        self.view = preview.PageView(pictures, size, key[2], order=self.order.current(),
                                     skip_empty=self.skip_empty.get())
        self.view.open(at_end=at_end)
        self.draw()

    # ---------------------------------------------------------------- reading
    def step(self, direction):
        if self.view is None:
            return
        result = self.view.step(direction)
        if result == 'next page':
            if self.page + 1 < self.pages:
                self.go_page(self.page + 1)
        elif result == 'previous page':
            if self.page > 0:
                self.go_page(self.page - 1, at_end=True)
        else:
            self.draw()

    def move(self, dx, dy):
        if self.view is not None:
            self.view.move_by(dx, dy)
            self.draw()

    def press(self, event):
        self.drag = (event.x, event.y, event.x, event.y, False)

    def motion(self, event):
        if self.drag is None or self.view is None:
            return
        x0, y0, last_x, last_y, moved = self.drag
        scale = self.scale.get()
        dx, dy = (last_x - event.x) // scale, (last_y - event.y) // scale
        if dx or dy:
            self.view.move_by(dx, dy)
            self.draw()
            self.drag = (x0, y0, last_x - dx * scale, last_y - dy * scale, True)

    def release(self, event):
        if self.drag is not None and not self.drag[4]:
            # A click: right half A, left half B.
            width = int(self.canvas.cget('width'))
            self.step(1 if event.x >= width // 2 else -1)
        self.drag = None

    def wheel(self, event, sideways=False):
        delta = -self.STEP if event.delta > 0 else self.STEP
        self.move(delta, 0) if sideways else self.move(0, delta)

    def draw(self):
        if self.view is None:
            return
        screen = self.view.render(page_map=self.page_map.get())
        scale = self.scale.get()
        shown = screen.resize((screen.width * scale, screen.height * scale), Image.NEAREST)
        self.canvas.configure(width=shown.width, height=shown.height)
        self.photo = ImageTk.PhotoImage(shown)
        self.canvas.delete('all')
        self.canvas.create_image(0, 0, image=self.photo, anchor='nw')
        v = self.view
        name = self.zoom.get().split()[0]
        self.status.configure(text=f'Page {self.page + 1} of {self.pages}  ·  {name}  ·  screen {v.tile + 1} of '
                                   f'{v.tiles}  ·  {v.sw} x {v.sh} pixels, as on the GBA')

    def close(self):
        if self.source is not None and not self.busy:
            self.source.close()
        self.main.preview_window = None
        self.top.destroy()


class Options:
    def __init__(self, **values):
        self.__dict__.update(values)


def main():
    root = tk.Tk()
    try:
        root.tk.call('tk', 'scaling', max(1.0, root.winfo_fpixels('1i') / 72.0))
    except tk.TclError:
        pass
    if sys.platform.startswith('win'):
        ttk.Style(root).theme_use('vista')
    elif 'clam' in ttk.Style(root).theme_names():
        ttk.Style(root).theme_use('clam')
    Window(root, [a for a in sys.argv[1:] if os.path.exists(a)])
    root.mainloop()


if __name__ == '__main__':
    main()
