"""
BSD Licensed
From
https://github.com/cloudpipe/multyvac-fork/blob/master/multyvac/util/module_dependency.py
"""

import ast
import imp
import logging
import pkgutil


class ModuleDependencyAnalyzer(object):
    _IMP_TYPE_NAMES = {
        imp.PY_FROZEN: 'frozen',
        imp.PY_SOURCE: 'source',
        imp.PY_COMPILED: 'compiled',
        imp.C_EXTENSION: 'c-extension',
        imp.C_BUILTIN: 'built-in',
    }

    def __init__(self):
        """Creates new ModuleDependencyAnalyzer"""
        self._logger = logging.getLogger('multyvac.dependency-analyzer')
        # Root modules that have been or are being inspected
        self._inspected_modules = set()
        # Root modules that have yet to be inspected
        self._modules_to_inspect = set()
        # Root modules that should be ignored by this (Not sent or traversed)
        self._modules_to_ignore = set()
        # Root module paths to transmit. Will never include path to modules
        # that contain c-extensions, and are thus untransmittable.
        self._paths_to_transmit = set()
        self.has_module_dependencies = False

    def add(self, module_name):
        """
        Adds a module to be analyzed.
        :param module_name: String of module name.
        """
        self._logger.debug('Queuing module %r', module_name)
        root_module_name = self._extract_root_module(module_name)
        self._modules_to_inspect.add(root_module_name)
        while self._modules_to_inspect:
            self._inspect(self._modules_to_inspect.pop())

    def ignore(self, module_name):
        """
        Ignores modules in dependency analysis so that they are neither
        transmitted nor traversed. Should be called before dependencies
        are sent the first time.
        """
        # what if module is already part of paths to transmit?
        if hasattr(module_name, '__iter__'):
            self._modules_to_ignore.update(module_name)
        elif isinstance(module_name, str):
            self._modules_to_ignore.add(module_name)
        else:
            raise TypeError('module_name must be string, found type {}'.format(
                type(module_name)))

    def get_and_clear_paths(self):
        # might be nice if this returned module names as well
        paths = self._paths_to_transmit
        if paths:
            self.has_module_dependencies = True
        self._paths_to_transmit = set()
        return paths

    def _inspect(self, root_module_name):
        """
        Determines what resources to send over (if any) for a given module.
        """
        if root_module_name in self._inspected_modules:
            self._logger.debug('Already inspected module %r, skipping',
                               root_module_name)
            return
        elif root_module_name in self._modules_to_ignore:
            self._logger.debug('Module %r is to be ignored, skipping',
                               root_module_name)
            return
        else:
            # Add module to set of scanned modules, before we've analyzed it
            self._inspected_modules.add(root_module_name)

        self._logger.debug('Inspecting module %r', root_module_name)
        try:
            fp, pathname, description = imp.find_module(root_module_name)
        except ImportError:
            self._logger.debug('Could not find module %r, skipping',
                               root_module_name)
            return
        suffix, mode, type = description
        if type == imp.PY_SOURCE:
            self._paths_to_transmit.add(pathname)
            self._logger.debug('Module %r is source/compiled. Added path %r',
                               root_module_name, pathname)
            # TODO: Does this work with compiled sources?
            try:
                source_imps = self._find_imports(
                    ast.parse(fp.read(), root_module_name))
            except SyntaxError:
                self._logger.debug('Module %r has a syntax error. '
                                   'Skipping source analysis',
                                   root_module_name)
                # For malformed source code
                source_imps = []
            # Close the file handle that's been opened for us by find_module
            fp.close()
            self._logger.debug('Module %r had these imports %r',
                               root_module_name, source_imps)
            for source_imp in source_imps:
                if source_imp in self._inspected_modules:
                    self._logger.debug('Module %r Source import %r '
                                       'already inspected', root_module_name,
                                       source_imp)
                elif source_imp in self._modules_to_inspect:
                    self._logger.debug('Module %r Source import %r '
                                       'already queued', root_module_name,
                                       source_imp)
                elif source_imp in self._modules_to_ignore:
                    self._logger.debug('Module %r Source import %r '
                                       'to be ignored', root_module_name,
                                       source_imp)
                else:
                    # Cannot be relative import since this is top-level
                    self._modules_to_inspect.add(source_imp)
                    self._logger.debug('Module %r Source import %r added '
                                       'to queue', root_module_name,
                                       source_imp)
        elif type == imp.PKG_DIRECTORY:
            self._logger.debug('Module %r is package. Recursing...',
                               root_module_name)
            if self._deep_inspect_path(pathname, root_module_name):
                self._paths_to_transmit.add(pathname)
                self._logger.debug(
                    'Module %r has no c-extensions. Added path %r',
                    root_module_name, pathname)
        elif type in (imp.C_EXTENSION, imp.C_BUILTIN, imp.PY_FROZEN,
                      imp.PY_COMPILED):
            self._logger.debug('Module %r is %s. Skipping.', root_module_name,
                               self._IMP_TYPE_NAMES[type])
        else:
            raise Exception('Unrecognized module %r type %s' %
                            (root_module_name, type))

    def _deep_inspect_path(self, path, package_name):
        """
        Traverses :param path: analyzing all valid Python modules.
        Returns True if this path is eligible to be sent (No c-extensions).
        Adds module references to list of modules to be inspected.
        """
        ret = True
        for _, submodule_name, is_pkg in pkgutil.iter_modules([path]):
            self._logger.debug('Inspecting submodule %r', submodule_name)
            fp, pathname, description = imp.find_module(submodule_name, [path])
            suffix, mode, type = description
            if type == imp.PY_SOURCE:
                self._logger.debug('%r -> %r is source/compiled. '
                                   'Scanning imports.', package_name,
                                   submodule_name)
                # TODO: Does this work with compiled sources?
                try:
                    source_imps = self._find_imports(
                        ast.parse(fp.read(), submodule_name))
                except SyntaxError:
                    self._logger.debug('%r -> %r has a syntax error. '
                                       'Skipping source analysis',
                                       package_name, submodule_name)
                    source_imps = []
                # Close the file handle that's been opened for us by find_module
                fp.close()
                self._logger.debug('%r -> %r had these imports %r',
                                   package_name, submodule_name, source_imps)
                for source_imp in source_imps:
                    if source_imp in self._inspected_modules:
                        self._logger.debug('%r -> %r -> %r already inspected',
                                           package_name, submodule_name,
                                           source_imp)
                    elif source_imp in self._modules_to_inspect:
                        self._logger.debug('%r -> %r -> %r already queued',
                                           package_name, submodule_name,
                                           source_imp)
                    elif source_imp in self._modules_to_ignore:
                        self._logger.debug('%r -> %r -> %r to be ignored',
                                           package_name, submodule_name,
                                           source_imp)
                    elif self._is_relative_import(source_imp, path):
                        self._logger.debug('%r -> %r -> %r is relative.',
                                           package_name, submodule_name,
                                           source_imp)
                    else:
                        self._modules_to_inspect.add(source_imp)
                        self._logger.debug('%r -> %r -> %r added to queue',
                                           package_name, submodule_name,
                                           source_imp)
            elif type == imp.PKG_DIRECTORY:
                self._logger.debug('%r -> %r is package. Recursing...',
                                   package_name, submodule_name)
                ret = ret and self._deep_inspect_path(pathname, package_name)
            elif type in (imp.C_EXTENSION, imp.C_BUILTIN, imp.PY_FROZEN,
                          imp.PY_COMPILED):

                self._logger.debug('%r -> %r is %s.', package_name,
                                   submodule_name, self._IMP_TYPE_NAMES[type])

                # Close the file handle that's been opened for us by find_module
                fp.close()

                # TODO: Can we go from compiled Python to an AST to identify
                # imports?
                # Since this is a common case, we assume that the PY will be
                # alongside the PYC for now, and ignore any issues that may
                # arise.
                if type != imp.PY_COMPILED:
                    ret = False
            else:
                raise Exception('Unrecognized module type %s' % submodule_name)

        return ret

    def _is_relative_import(self, module_name, path):
        """Checks if import is relative. Returns True if relative, False if
        absolute, and None if import could not be found."""
        try:
            # Check within the restricted path of a (sub-)package
            imp.find_module(module_name, [path])
        except ImportError:
            pass
        else:
            return True

        try:
            # Check across all of sys.path
            imp.find_module(module_name)
        except ImportError:
            pass
        else:
            return False

        # Module could not be found on system due to:
        # 1. Import that doesn't exist. "Bad import".
        # 2. Since we're only scanning the AST, there's a good chance the
        #    import's inclusion is conditional, and would never be triggered.
        #    For example, an import specific to an OS.
        return None

    def _extract_root_module(self, module_name):
        """Given a module name, returns only the root module by ignoring
        everything including and after the leftmost "." if one exists."""
        return module_name.split('.')[0]

    def _find_imports(self, node):
        """Recurses through AST collecting the targets of all import
        statements."""
        if isinstance(node, ast.Import):
            return {
                self._extract_root_module(alias.name)
                for alias in node.names
            }
        elif isinstance(node, ast.ImportFrom):
            # We ignore all imports with levels other than 0. That's because if
            # if level > 0, we know that it's a relative import, and we only
            # care about root modules.
            if node.level == 0:
                return {self._extract_root_module(node.module)}
            else:
                return set()
        elif hasattr(node, 'body') and hasattr(node.body, '__iter__'):
            # Not all bodies are lists (for ex. exec)
            imps = set()
            for child_node in node.body:
                imps.update(self._find_imports(child_node))
            return imps
        else:
            return set()
