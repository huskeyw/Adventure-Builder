import json
import struct

def to_c64_text(text, max_len):
    text = text.upper()
    text = text.replace('\n', '\x0D')
    encoded = text.encode('ascii', errors='ignore')[:max_len-1]
    return encoded.ljust(max_len, b'\0')

def build_data():
    print("Loading story.json...")
    try:
        with open('story.json', 'r') as f:
            data = json.load(f)
    except FileNotFoundError:
        print("ERROR: story.json not found!")
        return

    # Grab the intro story from JSON!
    intro_raw = data.get('intro', 'welcome to the adventure! find the gem and escape.')
    intro_c64 = to_c64_text(intro_raw, 512)

    rooms  = data.get('rooms',  [])
    items  = data.get('items',  [])
    actors = data.get('actors', [])

    num_rooms  = len(rooms)
    num_items  = len(items)
    num_actors = len(actors)

    print("Compiling boot.dat...")
    with open('boot.dat', 'wb') as out:
        # Pack the intro text right after the header counts
        out.write(struct.pack("<hhh512s", num_rooms, num_items, num_actors, intro_c64))

        for it in items:
            name = to_c64_text(it.get('name', 'UNKNOWN'), 32)
            desc = to_c64_text(it.get('desc', 'Empty'), 128)
            loc    = it.get('loc',  -99)
            i_type = it.get('type', 0)
            stat   = it.get('stat', 0)
            is_quest = it.get('is_quest', 0)
            out.write(struct.pack("<32s128shhhh", name, desc, loc, i_type, stat, is_quest))

        for act in actors:
            name = to_c64_text(act.get('name', 'UNKNOWN'), 32)
            desc = to_c64_text(act.get('desc', 'Empty'), 128)
            loc = act.get('loc', -99)
            hp  = act.get('hp',   1)
            dmg = act.get('dmg',  1)
            is_quest = act.get('is_quest', 0)
            out.write(struct.pack("<32s128shhhh", name, desc, loc, hp, dmg, is_quest))

    print("Compiling rooms.dat ({} rooms x 244 bytes)...".format(num_rooms))
    with open('rooms.dat', 'wb') as out:
        for r in rooms:
            name = to_c64_text(r.get('name', 'Unknown'), 32)
            desc = to_c64_text(r.get('desc', 'Empty'), 200)

            n = r.get('n', -99)
            s = r.get('s', -99)
            e = r.get('e', -99)
            w = r.get('w', -99)
            u = r.get('u', -99)
            d = r.get('d', -99)

            record = struct.pack("<32s200shhhhhh", name, desc, n, s, e, w, u, d)
            assert len(record) == 244, "Record must be 244 bytes!"
            out.write(record)

    print("Success!")

if __name__ == '__main__':
    build_data()