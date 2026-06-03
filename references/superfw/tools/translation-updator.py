#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import requests, argparse, time, io, zipfile, os

parser = argparse.ArgumentParser(prog='translation-updator')
parser.add_argument('--token', dest='token', required=True, help='Crowdin Personal token')
parser.add_argument('--project', dest='project', type=int, required=True, help='Project ID')
parser.add_argument('--bundle', dest='bundle', type=int, required=True, help='Bundle ID')
parser.add_argument('--outdir', dest='outdir', required=True, help='Output directory for the JSON files')
args = parser.parse_args()

r = requests.post("https://api.crowdin.com/api/v2/projects/%d/bundles/%d/exports" % (args.project, args.bundle),
                  headers={"Authorization": "Bearer " + args.token})
expid = r.json()["data"]["identifier"]

while True:
  r = requests.get("https://api.crowdin.com/api/v2/projects/%d/bundles/%d/exports/%s" % (args.project, args.bundle, expid),
                   headers={"Authorization": "Bearer " + args.token})
  if r.json()["data"]["status"] == "finished":
    break
  time.sleep(2)

r = requests.get("https://api.crowdin.com/api/v2/projects/%d/bundles/%d/exports/%s/download" % (args.project, args.bundle, expid),
                 headers={"Authorization": "Bearer " + args.token})
url = r.json()["data"]["url"]

# Download blob
blob = requests.get(url)

zd = io.BytesIO(blob.content)

with zipfile.ZipFile(zd) as ifd:
  for fn in ifd.namelist():
    open(os.path.join(args.outdir, fn), "wb").write(ifd.read(fn))


