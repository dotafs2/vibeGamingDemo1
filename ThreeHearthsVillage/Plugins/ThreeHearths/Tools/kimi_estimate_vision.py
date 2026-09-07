"""Estimate a scene observation using the official tokenizer, without generation.

Does not create/reset a budget or send a chat completion. Price assumptions are
the project's existing ledger prices, explicitly included in the output.
"""
import argparse
import base64
import json
import hashlib
from datetime import datetime, timezone
from pathlib import Path

from kimi_budget import NIGHT_POLICY
from kimi_gateway import KimiProvider
from kimi_vision import validate_png_url


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config',type=Path,required=True)
    parser.add_argument('--image',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    image_bytes=args.image.read_bytes()
    url='data:image/png;base64,'+base64.b64encode(image_bytes).decode()
    dimensions=validate_png_url(url)
    provider=KimiProvider(json.loads(args.config.read_text(encoding='utf-8-sig')))
    messages=[{'role':'system','content':'你是中世纪村民。按真实截图判断自家房屋与院子是否满足工作、休息与社交目标。只选已有的左右后方扩建意向；材料、预算与占地由游戏复核。不可把愿望或没有实现的资产当作事实。返回 JSON action_id 和不超过100字的第一人称 reason。'},
              {'role':'user','content':[{'type':'text','text':'我想要安静的住所、更大的作坊和能招待邻居的院子。请指出一处可见证据，选择满意(0)、向右(1)、向左(2)、向后(3)或提出一项待审需求(4)。'},
                                        {'type':'image_url','image_url':{'url':url}}]}]
    response=provider.request('/tokenizers/estimate-token-count',{'model':NIGHT_POLICY.model,'messages':messages})
    count=response.get('data',{}).get('total_tokens')
    if type(count) is not int or not 0<=count<=NIGHT_POLICY.input_ceiling:
        raise ValueError('Unverified tokenizer result')
    price=NIGHT_POLICY
    upper=(count*price.input_nano_per_token+256*price.output_nano_per_token)/1e9
    result={'queried_at_utc':datetime.now(timezone.utc).isoformat(),'image_sha256':hashlib.sha256(image_bytes).hexdigest(),'model':price.model,'image':args.image.name,'dimensions':dimensions,'estimated_input_tokens':count,
            'output_token_assumption':256,'one_call_estimate_cny':upper,'ten_calls_estimate_cny':upper*10,
            'six_hundred_calls_estimate_cny':upper*600,'price_cny_per_million':{'input':price.input_nano_per_token/1000,'output':price.output_nano_per_token/1000},
            'scope':'Sample prompt plus real image; actual resident context changes token count. No chat completion was sent. This does not reconstruct historical spend.'}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(result,ensure_ascii=False))


if __name__=='__main__': main()
