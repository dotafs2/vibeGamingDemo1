"""Read the official download page and report Windows package links only."""
import json
import urllib.request
from html.parser import HTMLParser
class Links(HTMLParser):
    def __init__(self): super().__init__(); self.links=[]
    def handle_starttag(self,tag,attrs):
        if tag=='a':
            href=dict(attrs).get('href','')
            if 'downloads.godotengine.org' in href and href not in self.links: self.links.append(href)
with urllib.request.urlopen('https://godotengine.org/download/windows/',timeout=25) as r:
    source=r.read(1500000).decode('utf-8')
p=Links();p.feed(source)
print(json.dumps(p.links,indent=2))
