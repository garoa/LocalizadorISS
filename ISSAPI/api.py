from flask import Flask
from flask_restful import Resource, Api
from skyfield.api import Topos, load
import os

# garante que estamos no diretorio correto
os.chdir('/home/daniel/ISSAPI')

# constantes
stations_url = 'http://celestrak.com/NORAD/elements/stations.txt'
garoa = Topos('23.569431 S', '46.699797 W')
minute = 1.0 / 24.0 / 60.0
second = minute / 60.0

# carrega arquivos
def loadFiles(reload=False):
    global iss, difference, ts
    satellites = load.tle(stations_url, reload=reload)
    iss = satellites['ISS (ZARYA)']
    difference = iss - garoa
    ts = load.timescale()

app = Flask(__name__)
api = Api(app)

# informa posicao atual da ISS 
# "lat|long"
class Position(Resource):
    def get(self):
        try:
            loadFiles()
            t = ts.now()
            geocentric = iss.at(t)
            subpoint = geocentric.subpoint()
            return str(subpoint.latitude.degrees) + "|" + str(subpoint.longitude.degrees) + "|" + str(int(subpoint.elevation.m))
        except Exception as ex:
            return str(ex)

# informa se ISS visivel ou nao e por quanto tempo
# "visivel|dias|horas|minutos|segundos"
class Passage(Resource):
    def visivel(self,tx):
        topocentric = difference.at(tx)
        alt, az, distance = topocentric.altaz()
        return (alt.degrees > 10) and (alt.degrees < 170), alt.degrees
    
    def get(self):
        loadFiles(True)
        t = ts.now()
        vis, altitude = self.visivel(t)
        v = '0'
        if vis:
            v = '1'
        t2 = t
        vis2 = vis
        while vis2 == vis:
            t2 = ts.tt_jd(t2.tt + minute)
            vis2, a = self.visivel(t2)
        while vis2 != vis:
            t2 = ts.tt_jd(t2.tt - second)
            vis2, a = self.visivel(t2)
        t3 = t2.tt - t.tt
        dias = int(t3)
        t3 = (t3 - dias) * 24.0
        horas = int(t3)
        t3 = (t3 - horas) * 60.0
        minutos = int(t3)
        t3 = (t3 - minutos) * 60.0
        segundos = int(t3)
        return str(v + "|" + str(dias) + "|" + str(horas) + "|" + str(minutos) + "|" + str(segundos))

class Version(Resource):
    def get(self):
        return '1.00'

api.add_resource(Position, '/', '/position')
api.add_resource(Passage, '/passage')
api.add_resource(Version, '/version')

if __name__ == '__main__':
    app.run(debug=True, host= '0.0.0.0')
