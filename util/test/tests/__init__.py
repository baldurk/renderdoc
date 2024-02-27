import pkgutil

__all__ = []
for loader, module_name, is_pkg in pkgutil.walk_packages(__path__):
    __all__.append(module_name)
    module = loader.find_spec(module_name).loader.load_module(module_name)
    globals()[module_name] = module
