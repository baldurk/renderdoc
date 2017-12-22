#!coding: utf-8
import re
from docutils import nodes
from docutils.transforms import Transform
import os
from sphinx.util.osutil import copyfile
from sphinx.util.console import bold
from sphinx.domains.python import PyXRefRole
from sphinx.domains.python import PythonDomain
from distutils.version import LooseVersion
from sphinx import __version__
# the searchindex.js system relies upon the object types
# in the PythonDomain to create search entries
from sphinx.domains import ObjType

PythonDomain.object_types['parameter'] = ObjType('parameter', 'param')


def _is_html(app):
    return app.builder.name in ('html', 'readthedocs')


def _tempdata(app):
    if '_sphinx_paramlinks_index' in app.env.indexentries:
        idx = app.env.indexentries['_sphinx_paramlinks_index']
    else:
        app.env.indexentries['_sphinx_paramlinks_index'] = idx = {}
    return idx


def autodoc_process_docstring(app, what, name, obj, options, lines):
    # locate :param: lines within docstrings.  augment the parameter
    # name with that of the parent object name plus a token we can
    # spot later.  Also put an index entry in a temporary collection.

    idx = _tempdata(app)

    docname = app.env.temp_data['docname']
    if docname in idx:
        doc_idx = idx[docname]
    else:
        idx[docname] = doc_idx = []

    def _cvt_param(name, line):
        if name.endswith(".__init__"):
            # kill off __init__ if present, the links are always
            # off the class
            name = name[0:-9]

        def cvt(m):
            modifier, objname, paramname = m.group(1) or '', name, m.group(2)
            refname = _refname_from_paramname(paramname, strip_markup=True)
            item = ('single', '%s (%s parameter)' % (refname, objname),
                    '%s.params.%s' % (objname, refname), '')
            if LooseVersion(__version__) >= LooseVersion('1.4.0'):
                item += (None,)
            doc_idx.append(item)
            return ":param %s_sphinx_paramlinks_%s.%s:" % (
                modifier, objname, paramname)
        return re.sub(r'^:param ([^:]+? )?([^:]+?):', cvt, line)

    if what in ('function', 'method', 'class'):
        lines[:] = [_cvt_param(name, line) for line in lines]


def _refname_from_paramname(paramname, strip_markup=False):
    literal_match = re.match(r'^``(.+?)``$', paramname)
    if literal_match:
        paramname = literal_match.group(1)
    refname = paramname
    eq_match = re.match(r'(.+?)=.+$', refname)
    if eq_match:
        refname = eq_match.group(1)
    if strip_markup:
        refname = re.sub(r'\\', '', refname)
    return refname


class LinkParams(Transform):
    # apply references targets and optional references
    # to nodes that contain our target text.
    default_priority = 210

    def apply(self):
        is_html = _is_html(self.document.settings.env.app)

        # seach <strong> nodes, which will include the titles for
        # those :param: directives, looking for our special token.
        # then fix up the text within the node.
        for ref in self.document.traverse(nodes.strong):
            text = ref.astext()
            if text.startswith("_sphinx_paramlinks_"):
                components = re.match(r'_sphinx_paramlinks_(.+)\.(.+)$', text)
                location, paramname = components.group(1, 2)
                refname = _refname_from_paramname(paramname)

                refid = "%s.params.%s" % (location, refname)
                ref.parent.insert(
                    0,
                    nodes.target('', '', ids=[refid])
                )
                del ref[0]

                ref.insert(0, nodes.Text(paramname, paramname))

                if is_html:
                    # add the "p" thing only if we're the HTML builder.

                    # using a real ¶, surprising, right?
                    # http://docutils.sourceforge.net/FAQ.html#how-can-i-represent-esoteric-characters-e-g-character-entities-in-a-document

                    # "For example, say you want an em-dash (XML
                    # character entity &mdash;, Unicode character
                    # U+2014) in your document: use a real em-dash.
                    # Insert concrete characters (e.g. type a real em-
                    # dash) into your input file, using whatever
                    # encoding suits your application, and tell
                    # Docutils the input encoding. Docutils uses
                    # Unicode internally, so the em-dash character is
                    # a real em-dash internally."   OK !

                    for pos, node in enumerate(ref.parent.children):
                        # try to figure out where the node with the
                        # paramname is. thought this was simple, but
                        # readthedocs proving..it's not.
                        # TODO: need to take into account a type name
                        # with the parens.
                        if isinstance(node, nodes.TextElement) and \
                                node.astext() == paramname:
                            break
                    else:
                        return

                    ref.parent.insert(
                        pos + 1,
                        nodes.reference(
                            '', '',
                            nodes.Text(u"#", u"#"),
                            refid=refid,
                            # paramlink is our own CSS class, headerlink
                            # is theirs.  Trying to get everything we can for
                            # existing symbols...
                            classes=['paramlink', 'headerlink']
                        )
                    )


def lookup_params(app, env, node, contnode):
    # here, we catch the "pending xref" nodes that we created with
    # the "paramref" role we added.   The resolve_xref() routine
    # knows nothing about this node type so it never finds anything;
    # the Sphinx BuildEnvironment then gives us one more chance to do a lookup
    # here.

    if node['reftype'] != 'paramref':
        return None

    target = node['reftarget']

    tokens = target.split(".")
    resolve_target = ".".join(tokens[0:-1])
    paramname = tokens[-1]

    # emulate the approach within
    # sphinx.environment.BuildEnvironment.resolve_references
    try:
        domain = env.domains[node['refdomain']]  # hint: this will be 'py'
    except KeyError:
        return None

    # BuildEnvironment doesn't pass us "fromdocname" here as the
    # fallback, oh well
    refdoc = node.get('refdoc', None)

    # we call the same "resolve_xref" that BuildEnvironment just tried
    # to call for us, but we load the call with information we know
    # it can find, e.g. the "object" role (or we could use :meth:/:func:)
    # along with the classname/methodname/funcname minus the parameter
    # part.

    for search in ["meth", "class", "func"]:
        newnode = domain.resolve_xref(
            env, refdoc, app.builder,
            search, resolve_target, node, contnode)
        if newnode is not None:
            break

    if newnode is not None:
        # assuming we found it, tack the paramname back onto to the final
        # URI.
        if 'refuri' in newnode:
            newnode['refuri'] += ".params." + paramname
        elif 'refid' in newnode:
            newnode['refid'] += ".params." + paramname
    return newnode


def add_stylesheet(app):
    app.add_stylesheet('sphinx_paramlinks.css')


def copy_stylesheet(app, exception):
    app.info(
        bold('The name of the builder is: %s' % app.builder.name), nonl=True)

    if not _is_html(app) or exception:
        return
    app.info(bold('Copying sphinx_paramlinks stylesheet... '), nonl=True)

    source = os.path.abspath(os.path.dirname(__file__))

    # the '_static' directory name is hardcoded in
    # sphinx.builders.html.StandaloneHTMLBuilder.copy_static_files.
    # would be nice if Sphinx could improve the API here so that we just
    # give it the path to a .css file and it does the right thing.
    dest = os.path.join(app.builder.outdir, '_static', 'sphinx_paramlinks.css')
    copyfile(os.path.join(source, "sphinx_paramlinks.css"), dest)
    app.info('done')


def build_index(app, doctree):
    entries = _tempdata(app)

    for docname in entries:
        doc_entries = entries[docname]
        app.env.indexentries[docname].extend(doc_entries)

        for entry in doc_entries:
            sing, desc, ref, extra = entry[:4]
            app.env.domains['py'].data['objects'][ref] = (docname, 'parameter')

    app.env.indexentries.pop('_sphinx_paramlinks_index')


def setup(app):
    app.add_transform(LinkParams)

    # PyXRefRole is what the sphinx Python domain uses to set up
    # role nodes like "meth", "func", etc.  It produces a "pending xref"
    # sphinx node along with contextual information.
    app.add_role_to_domain("py", "paramref", PyXRefRole())

    app.connect('autodoc-process-docstring', autodoc_process_docstring)
    app.connect('builder-inited', add_stylesheet)
    app.connect('build-finished', copy_stylesheet)
    app.connect('missing-reference', lookup_params)
    app.connect('doctree-read', build_index)
