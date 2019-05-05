author = u'David Wilson'
copyright = u'2019, David Wilson'
exclude_patterns = ['build']
html_show_sourcelink = False
html_show_sphinx = False
html_sidebars = {'**': ['globaltoc.html', 'github.html']}
html_static_path = ['static']
html_theme_path = ['.']
html_theme = 'alabaster'
html_theme_options = {
    'font_family': "Georgia, serif",
    'head_font_family': "Georgia, serif",
    'fixed_sidebar': True,
    'show_powered_by': False,
    'pink_2': 'fffafaf',
    'pink_1': '#fff0f0',
}
htmlhelp_basename = 'csvmonkeydoc'
language = None
master_doc = 'index'
project = u'csvmonkey'
pygments_style = 'sphinx'
source_suffix = '.rst'
templates_path = ['templates']
todo_include_todos = False
version = '0.0.2'
release = version
