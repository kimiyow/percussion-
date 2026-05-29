import time

#  general
DURACION_ACTIVO_MS  = 80    # duración del pulso _activo post-golpe

# bombo / tom 
NIVEL2_BOMBO        = 1000  
NIVEL_FUERTE_BOMBO  = 1750  
NIVEL2_TOM          = 1000
NIVEL_FUERTE_TOM    = 1550
TIMEOUT_EVENTO_MS   = 400   

# bongos
VENTANA_ENERGIA_MS  = 250   
UMBRAL_ACENTO_BONGO = 900   
PICO_MAX_BONGO      = 2500
BLOQUEO_B2_MS       = 30    

# tumba
PICO_MIN_TUMBA      = 150   
PICO_MAX_TUMBA      = 2500
BLOQUEO_TUMBA_MS    = 100   

# woodblocks 
NIVEL2_WB           = 1200
NIVEL3_WB           = 1650
NIVEL4_WB           = 1900


NOMBRES_INST = {
    0: "bombo", 1: "tom",    2: "tumba",
    3: "bongo1", 4: "bongo2",
    5: "wb1", 6: "wb2", 7: "wb3", 8: "wb4", 9: "wb5",
}


def onReceive(dat, rowIndex, message, bytes):
    procesar(dat, message)

def onReceiveLine(dat, rowIndex, message):
    procesar(dat, message)

def onReceiveRow(dat, rowIndex, rowArray):
    pass


def set_val(tabla, nombre, valor):
    for r in range(tabla.numRows):
        if tabla[r, 0].val == nombre:
            tabla[r, 1].val = valor
            return
    tabla.appendRow([nombre, valor])


def calcular_nivel_bombo_tom(pico, nivel2, nivel_fuerte):
    # devuelve 1, 2 o 3 (3 fusiona f y fff).
    if pico < nivel2:       return 1
    if pico < nivel_fuerte: return 2
    return 3


def procesar_bombo_tom(dat, tabla, canal, inst, pico, ahora, nivel2, nivel_fuerte):
    nivel = calcular_nivel_bombo_tom(pico, nivel2, nivel_fuerte)
    estado = dat.fetch(f'estado_{canal}', {'ultimo_n1': 0, 'ultimo_n2': 0})

    if nivel == 1:
        estado['ultimo_n1'] = ahora
    elif nivel == 2:
        estado['ultimo_n1'] = ahora  
        estado['ultimo_n2'] = ahora
    elif nivel == 3:
        estado['ultimo_n1'] = ahora
        estado['ultimo_n2'] = ahora

    dat.store(f'estado_{canal}', estado)

    evento_1      = 1 if (ahora - estado['ultimo_n1']) < TIMEOUT_EVENTO_MS else 0
    evento_2      = 1 if (ahora - estado['ultimo_n2']) < TIMEOUT_EVENTO_MS else 0
    evento_fuerte = 1 if nivel == 3 else 0

    set_val(tabla, f"{inst}_pico",          pico)
    set_val(tabla, f"{inst}_nivel",         nivel)
    set_val(tabla, f"{inst}_evento_1",      evento_1)
    set_val(tabla, f"{inst}_evento_2",      evento_2)
    set_val(tabla, f"{inst}_evento_fuerte", evento_fuerte)
    set_val(tabla, f"{inst}_activo",        1)
    dat.store(f'activo_ts_{canal}', ahora)


def procesar_bongo(dat, tabla, canal, inst, pico, ahora):
    if canal == 4:  # bongo2
        ts_b1 = dat.fetch('ultimo_golpe_b1', 0)
        if (ahora - ts_b1) < BLOQUEO_B2_MS:
            return  

    if canal == 3:  # bongo1
        dat.store('ultimo_golpe_b1', ahora)

    acc = dat.fetch(f'acc_{canal}', {
        'energia_suma': 0.0, 'golpes': 0,
        'acento': 0, 't_inicio': ahora, 'ultimo_golpe': 0,
    })

    pico_norm = min(pico / PICO_MAX_BONGO, 1.0)
    acc['energia_suma'] += pico_norm
    acc['golpes']       += 1
    acc['ultimo_golpe']  = ahora
    if pico >= UMBRAL_ACENTO_BONGO:
        acc['acento'] = 1

    dt = ahora - acc['t_inicio']
    if dt >= VENTANA_ENERGIA_MS:
        energia  = min(acc['energia_suma'] / acc['golpes'], 1.0) if acc['golpes'] > 0 else 0.0
        densidad = acc['golpes'] / (dt / 1000.0) if dt > 0 else 0

        set_val(tabla, f"{inst}_energia",  round(energia, 3))
        set_val(tabla, f"{inst}_densidad", round(densidad, 1))
        set_val(tabla, f"{inst}_acento",   acc['acento'])

        acc = {'energia_suma': 0.0, 'golpes': 0, 'acento': 0,
               't_inicio': ahora, 'ultimo_golpe': ahora}

    dat.store(f'acc_{canal}', acc)
    set_val(tabla, f"{inst}_pico",   pico)
    set_val(tabla, f"{inst}_activo", 1)
    dat.store(f'activo_ts_{canal}', ahora)


def procesar_tumba(dat, tabla, canal, inst, pico, ahora):
    ts_bombo = dat.fetch('ultimo_golpe_bombo', 0)
    if (ahora - ts_bombo) < BLOQUEO_TUMBA_MS:
        return 

    if pico < PICO_MIN_TUMBA:
        return 

    continuo = (pico - PICO_MIN_TUMBA) / (PICO_MAX_TUMBA - PICO_MIN_TUMBA)
    continuo = round(min(max(continuo, 0.0), 1.0), 3)

    set_val(tabla, f"{inst}_pico",     pico)
    set_val(tabla, f"{inst}_continuo", continuo)
    set_val(tabla, f"{inst}_activo",   1)
    dat.store(f'activo_ts_{canal}', ahora)


def procesar_woodblock(dat, tabla, canal, inst, pico, ahora):
    if pico < 120: return
    if pico < NIVEL2_WB:   nivel = 1
    elif pico < NIVEL3_WB: nivel = 2
    elif pico < NIVEL4_WB: nivel = 3
    else:                  nivel = 4

    set_val(tabla, f"{inst}_pico",   pico)
    set_val(tabla, f"{inst}_nivel",  nivel)
    set_val(tabla, f"{inst}_activo", 1)
    dat.store(f'activo_ts_{canal}', ahora)


def procesar(dat, message):
    try:
        partes = message.strip().split(":")
        if len(partes) != 2:
            return

        canal = int(partes[0])
        pico  = float(partes[1])
        inst  = NOMBRES_INST.get(canal, f"aux_{canal}")
        
        # validar que la tabla exista
        tabla = op('tabla_piezos')
        if not tabla: 
            print("ADVERTENCIA: No se encontró el nodo 'tabla_piezos'. Revisa el nombre.")
            return

        ahora = time.time() * 1000

        if canal == 0:   # bombo
            dat.store('ultimo_golpe_bombo', ahora)
            procesar_bombo_tom(dat, tabla, canal, inst, pico, ahora, NIVEL2_BOMBO, NIVEL_FUERTE_BOMBO)
        elif canal == 1: # tom
            procesar_bombo_tom(dat, tabla, canal, inst, pico, ahora, NIVEL2_TOM, NIVEL_FUERTE_TOM)
        elif canal == 2: # tumba
            procesar_tumba(dat, tabla, canal, inst, pico, ahora)
        elif canal in (3, 4): # bongos
            procesar_bongo(dat, tabla, canal, inst, pico, ahora)
        else:            # woodblocks
            procesar_woodblock(dat, tabla, canal, inst, pico, ahora)

    except Exception as e:
    print(f"ERROR EN PYTHON procesando '{message}': {e}")
