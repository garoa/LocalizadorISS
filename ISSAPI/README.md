# ISSAPI

## APIs implementadas:

GET /position: Retorna "lat|long“

GET /passage: Retorna "visivel|dias|horas|minutos|segundos“

## Preparação do ambiente

```
cd ISSAPI
python3 -m venv venv
source venv/bin/activate
pip install flask
pip install flask-restful
pip install skyfield
pip install flup
```

## Execução

Foi instalado em um servidor com lighttpd

## Referências adicionais:

https://rhodesmill.org/skyfield/
https://flask-restful.readthedocs.io/en/latest/
