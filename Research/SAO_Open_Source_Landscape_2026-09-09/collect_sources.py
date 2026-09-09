"""Bounded read-only public repository survey; never executes downloaded code."""
import concurrent.futures
import datetime
import hashlib
import json
import pathlib
import re
import sys
import threading
import urllib.error
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent
API_BLOCKED = threading.Event()
REPOS = """
a16z-infra/ai-town
joonspk-research/generative_agents
google-deepmind/concordia
tsinghua-fib-lab/AgentSociety
camel-ai/oasis
mindcraft-bots/mindcraft
MineDojo/Voyager
altera-al/project-sid
SimWorld-AI/SimWorld
SimWorld-AI/SimWorld-Studio
EricSun0218/OpenGameAgent
JustInternetAI/AgentArena
undreamai/LLMUnity
getnamo/Llama-Unreal
lschiweck/LLM-NPC-Agents
Agentshire/Agentshire
elizaOS/eliza
mem0ai/mem0
getzep/graphiti
langchain-ai/langgraph
BerriAI/litellm
ggml-org/llama.cpp
vrm-c/UniVRM
ruyo/VRM4U
V-Sekai/godot-vrm
pixiv/three-vrm
Unity-Technologies/com.unity.toonshader
saturday06/VRM-Addon-for-Blender
GodotVR/godot-xr-tools
Unity-Technologies/XR-Interaction-Toolkit-Examples
KhronosGroup/OpenXR-SDK-Source
heroiclabs/nakama
MirrorNetworking/Mirror
colyseus/colyseus
crashkonijn/GOAP
luxkun/ReGoap
limbonaut/limboai
Unity-Technologies/ml-agents
unrealcv/unrealcv
ahujasid/blender-mcp
CoplayDev/unity-mcp
chongdashu/unreal-mcp
princeton-vl/infinigen
microsoft/TRELLIS.2
Tencent-Hunyuan/Hunyuan3D-2.1
VAST-AI-Research/TripoSR
godotengine/godot
mrdoob/three.js
recastnavigation/recastnavigation
atteneder/glTFast
rdeioris/glTFRuntime
echo-yiyiyi/cube
hecomi/uLipSync
k2-fsa/sherpa-onnx
ggml-org/whisper.cpp
TokisanGames/Terrain3D
NVIDIA/ACE
NVIDIA/game-agent-sdk
NVIDIA/Audio2Face-3D-SDK
""".strip().splitlines()

def get(url, cap=131072):
    request = urllib.request.Request(url, headers={"User-Agent": "SAO-repository-survey/1.0", "Accept": "application/vnd.github+json"})
    with urllib.request.urlopen(request, timeout=15) as response:
        data = response.read(cap + 1)
        return data[:cap], len(data) > cap, response.geturl()

def inspect(repo):
    result = {"requested_repository": repo, "retrieved_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(), "execution_tested": False}
    try:
        if API_BLOCKED.is_set():
            raise RuntimeError("Metadata skipped after public API rate limit")
        raw, truncated, url = get("https://api.github.com/repos/" + repo)
        metadata = json.loads(raw)
        result.update({key: metadata.get(key) for key in ["full_name", "html_url", "description", "default_branch", "created_at", "pushed_at", "archived", "disabled", "language", "license"]})
        result["metadata_source"] = url
    except Exception as error:
        result["metadata_error"] = str(error)
        if isinstance(error, urllib.error.HTTPError) and error.code in (403, 429):
            API_BLOCKED.set()
    branch = result.get("default_branch") or "HEAD"
    for filename in ["README.md", "README.rst", "readme.md"]:
        url = f"https://raw.githubusercontent.com/{repo}/{branch}/{filename}"
        try:
            raw, truncated, final_url = get(url, 98304)
            content = raw.decode("utf-8", "replace")
            result.update({"readme_source": final_url, "readme_sha256_of_read_bytes": hashlib.sha256(raw).hexdigest(), "readme_truncated": truncated, "readme_bytes_read": len(raw)})
            return result
        except urllib.error.HTTPError as error:
            if error.code != 404:
                result["readme_error"] = str(error)
                break
        except Exception as error:
            result["readme_error"] = str(error)
            break
    return result

def inspect_license(item):
    repo = item["requested_repository"]
    filenames = ["LICENSE", "LICENSE.md", "LICENSE.txt", "COPYING"]
    for filename in filenames:
        url = f"https://raw.githubusercontent.com/{repo}/{item.get('default_branch') or 'HEAD'}/{filename}"
        try:
            raw, truncated, final_url = get(url, 32768)
            content = raw.decode("utf-8", "replace")
            found = []
            for pattern, name in [(r"Unity Companion", "Unity Companion"), (r"Hunyuan.*Community|TENCENT HUNYUAN", "Tencent Hunyuan Community"), (r"GNU GENERAL PUBLIC LICENSE", "GPL (check version and exceptions)"), (r"GNU LESSER GENERAL", "LGPL (check version)"), (r"Apache License", "Apache-2.0"), (r"MIT License|Permission is hereby granted, free of charge", "MIT-style"), (r"Redistribution and use in source and binary forms", "BSD-style (check clauses)"), (r"provided .as-is.|zlib", "zlib-style (check terms)")]:
                if re.search(pattern, content, re.I | re.S):
                    found.append(name)
            item["license_file_source"] = final_url
            item["license_file_sha256"] = hashlib.sha256(raw).hexdigest()
            item["license_detected_names_not_legal_conclusion"] = found or ["Custom or unclassified; inspect source"]
            break
        except urllib.error.HTTPError as error:
            if error.code != 404:
                item["license_fetch_error"] = str(error)
                break
        except Exception as error:
            item["license_fetch_error"] = str(error)
            break
    item.pop("readme_evidence_lines", None)
    return item

if __name__ == "__main__" and "--finish-catalog" in sys.argv:
    API_BLOCKED.set()
    path = ROOT / "repository_metadata.json"
    output = json.loads(path.read_text(encoding="utf-8"))
    existing = {item["requested_repository"] for item in output["repositories"]}
    missing = [repo for repo in REPOS if repo not in existing]
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        additions = list(pool.map(inspect, missing))
        additions = list(pool.map(inspect_license, additions))
    output["repositories"].extend(additions)
    path.write_text(json.dumps(output, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for item in additions:
        print(item["requested_repository"], item.get("license_detected_names_not_legal_conclusion"), item.get("readme_source"))
elif __name__ == "__main__" and "--licenses-only" in sys.argv:
    path = ROOT / "repository_metadata.json"
    output = json.loads(path.read_text(encoding="utf-8"))
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        output["repositories"] = list(pool.map(inspect_license, output["repositories"]))
    output["metadata_availability_note"] = "Public GitHub repository API was rate limited. Missing dates and archive status are unknown, not evidence of inactivity. README/source browsing was used instead."
    path.write_text(json.dumps(output, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for item in output["repositories"]:
        print(item["requested_repository"], item.get("license_detected_names_not_legal_conclusion"), item.get("license_fetch_error", ""))
elif __name__ == "__main__":
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(inspect, REPOS))
    output = {"scope": "Curated representative landscape, not an exhaustive index of GitHub", "method": "Public repository metadata and bounded README inspection; no repositories executed", "repositories": results}
    (ROOT / "repository_metadata.json").write_text(json.dumps(output, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for item in results:
        license_info = item.get("license") or {}
        print(json.dumps({"repo": item["requested_repository"], "resolved": item.get("full_name"), "license": license_info.get("spdx_id"), "archived": item.get("archived"), "pushed_at": item.get("pushed_at"), "readme": bool(item.get("readme_source")), "error": item.get("metadata_error") or item.get("readme_error")}, ensure_ascii=False))
