import requests
import sqlite3
import datetime
import random
import json
db = '__HOME__/team29/commands.db'
authorized_users = ['ch3nj']

commands = {0: 'forward', 1: 'back', 2: 'left', 3: 'right'}

def request_handler(request):
    random.seed()
    if request['method'] == 'GET':
        values = request['values']
        if values['type'] == 'command':
            conn = sqlite3.connect(db)
            c = conn.cursor()
            out = None
            try:
                c.execute('''CREATE TABLE command_table (command int, timing timestamp);''')
                out = "database constructed"
            except:
                recent = datetime.datetime.now()- datetime.timedelta(seconds = 15)
                recent_commands = c.execute('''SELECT * FROM command_table WHERE timing > ? ORDER BY timing DESC''',(recent,)).fetchall()
                if len(recent_commands) == 0:
                    out = 'stop'
                else:
                    out = commands[recent_commands[random.randint(0, len(recent_commands)-1)][0]]
            conn.commit()
            conn.close()
            return out
        if values['type'] == 'graph':
            conn = sqlite3.connect(db)
            c = conn.cursor()
            out = []
            recent = datetime.datetime.now()- datetime.timedelta(seconds = 15)
            for num in commands:
                out.append({'command': commands[num], 'amount': len(c.execute('''SELECT * FROM command_table WHERE command = ? AND timing > ? ORDER BY timing DESC''',(num, recent)).fetchall())})
            conn.close()
            return json.dumps(out)

    elif request['method'] == 'POST':
        conn = sqlite3.connect(db)
        c = conn.cursor()
        command = request['form']['command']
        c.execute('''INSERT into command_table VALUES (?,?)''',(command, datetime.datetime.now())).fetchall()
        conn.commit()
        conn.close()
        return "done"
