(function(exports, importNativeModule) {
    "use strict";

    const Importer = importNativeModule('_importer');
    const Gio = Importer.importGIModule('Gio', '2.0');

    let _exports = {};

    // This is where "unknown namespace" classes live, where we don't
    // have info for them. One common example is GLocalFile.
    _exports['gi/__gjsPrivateNS'] = {};

    function loadNativeModule(moduleID) {
        _exports[moduleID] = importNativeModule(moduleID);
    }

    function runOverridesForGIModule(module, moduleID) {
        let overridesModuleName = ('overrides/' + moduleID);
        loadJSModule(overridesModuleName);
        let overridesModule = _exports[overridesModuleName];
        if (!overridesModule)
            return;

        let initFunc = overridesModule._init;
        if (!initFunc)
            return;

        initFunc.call(module);
    }

    function importGIModuleWithOverrides(moduleID, moduleVersion) {
        let exportedID = 'gi/' + moduleID;
        if (_exports[exportedID])
            return _exports[exportedID];

        let module = Importer.importGIModule(moduleID, moduleVersion);
        _exports[exportedID] = module;
        _exports[exportedID + '/' + moduleVersion] = module;
        runOverridesForGIModule(module, moduleID);
        return module;
    }

    function loadGIModule(moduleID) {
        if (!moduleID.startsWith('gi/'))
            return;

        let giModuleID = moduleID.slice(3);

        let moduleVersion;
        if (giModuleID.indexOf('/') >= 0)
            [giModuleID, moduleVersion] = moduleVersion.split('/', 2);
        else
            moduleVersion = null;

        importGIModuleWithOverrides(giModuleID, moduleVersion);
    }

    function createModuleScope(id, uri) {
        let module = {};

        Object.defineProperty(module, "id", { value: id,
                                              configurable: false,
                                              writable: false });
        Object.defineProperty(module, "uri", { value: uri,
                                               configurable: false,
                                               writable: false });

        let scope = {};
        scope.module = module;

        // XXX -- for compatibility with the old module system, we don't
        // give modules their own private namespace, but simply export
        // the module scope directly.
        //
        // This should eventually go away, when we fully adopt CommonJS.
        scope.exports = scope;

        return scope;
    }

    function loadJSModule(moduleID) {
        function getModuleContents(modulePath) {
            let file = Gio.File.new_for_commandline_arg(modulePath);

            let success, script;
            try {
                [success, script] = file.load_contents(null);
            } catch(e) {
                return null;
            }

            return script;
        }

        function evalModule(modulePath, script) {
            let scope = createModuleScope(moduleID, modulePath);
            _exports[moduleID] = scope;

            let evalSuccess = false;
            try {
                // Don't catch errors for the eval, as those should propagate
                // back up to the user...
                Importer.evalWithScope(scope, script, modulePath);
                evalSuccess = true;
            } finally {
                if (!evalSuccess)
                    delete _exports[moduleID];
            }
        }

        for (let path of require.paths) {
            let modulePath = path + '/' + moduleID + '.js';
            let script = getModuleContents(modulePath);
            if (!script)
                continue;

            evalModule(modulePath, script);
        }
    }

    let require = exports.require = function require(moduleID) {
        if (_exports[moduleID])
            return _exports[moduleID];

        const FINDERS = [
            loadNativeModule,
            loadGIModule,
            loadJSModule,
        ];

        for (let finder of FINDERS) {
            finder(moduleID);
            if (_exports[moduleID])
                return _exports[moduleID];
        }

        throw new Error("Could not load module '" + moduleID + "'");
    }
    require.paths = Importer.getBuiltinSearchPath();

    function installCompatImports() {
        // imports.gi
        let gi = new Proxy({
            versions: {},
        }, {
            get: function(target, name) {
                if (target[name])
                    return target[name];

                let version = target.versions[name] || null;
                return importGIModuleWithOverrides(name, version);
            },
        });

        function importModule(module, file) {
            let success, script;
            try {
                [success, script] = file.load_contents(null);
            } catch(e) {
                return null;
            }

            // Don't catch errors for the eval, as those should propagate
            // back up to the user...
            Importer.evalWithScope(module, script, file.get_parse_name());
            return module;
        }

        function importFile(parent, name, file) {
            let module = {};
            module.__file__ = file.get_parse_name();
            module.__moduleName__ = name;
            module.__parentModule__ = parent;
            importModule(module, file);
            return module;
        }

        function importDirectory(parent, name) {
            let searchPath = parent.searchPath.map(function(path) {
                return path + '/' + name;
            }).filter(function(path) {
                let file = Gio.File.new_for_commandline_arg(path);
                let type = file.query_file_type(Gio.FileQueryInfoFlags.NONE, null);
                return (type == Gio.FileType.DIRECTORY);
            });

            let module = createSearchPathImporter();
            module.searchPath = searchPath;
            module.__moduleName__ = name;
            module.__parentModule__ = parent;

            function runInit() {
                for (let path of searchPath) {
                    let initFile = Gio.File.new_for_commandline_arg(path + '/__init__.js');
                    if (initFile.query_exists(null)) {
                        importModule(module, initFile);
                        return;
                    }
                }
            }

            runInit();

            return module;
        }

        function tryImport(proxy, name) {
            function tryPath(path) {
                let file, type;
                file = Gio.File.new_for_commandline_arg(path);
                type = file.query_file_type(Gio.FileQueryInfoFlags.NONE, null);
                if (type == Gio.FileType.DIRECTORY)
                    return importDirectory(proxy, name);

                file = Gio.File.new_for_commandline_arg(path + '.js');
                if (file.query_exists(null))
                    return importFile(proxy, name, file);

                return null;
            }

            for (let path of proxy.searchPath) {
                let modulePath = path + '/' + name;
                let obj = tryPath(modulePath);
                if (obj)
                    return obj;
            }
            return null;
        }

        function createSearchPathImporter() {
            let proxy = new Proxy({}, {
                get: function(target, name) {
                    if (target[name])
                        return target[name];

                    let obj = tryImport(proxy, name);
                    target[obj] = obj;
                    return obj;
                },
            });
            return proxy;
        }

        let rootDirectoryImporter = createSearchPathImporter();
        rootDirectoryImporter.searchPath = Importer.getBuiltinSearchPath();

        // root importer, checks for native modules
        let rootImporter = new Proxy(rootDirectoryImporter, {
            get: function(target, name) {
                if (target[name])
                    return target[name];

                let nativeModule = importNativeModule(name);
                if (nativeModule) {
                    target[name] = nativeModule;
                    return nativeModule;
                }

                return rootDirectoryImporter[name];
            },
        });
        rootImporter.gi = gi;

        exports.imports = rootImporter;
    }
    installCompatImports();

})(window, importNativeModule);
