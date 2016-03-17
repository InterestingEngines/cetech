from playground.engine.consoleproxy import ConsoleProxy


class ConsoleAPI(ConsoleProxy):
    def wait(self):
        self.send_command('wait')
        pass

    def lua_execute(self, script):
        self.send_command('lua.execute', script=script)

    def compile_all(self):
        self.send_command('resource_compiler.compile_all')

    def autocomplete_list(self):
        #self.lua_execute('autocomplite_list()')
        pass

    def resize(self, w, h):
        self.send_command('renderer.resize', width=w, height=h)
        pass