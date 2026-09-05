"""Offline validation of the private bridge boundary. No model calls or credentials."""
import copy
import json
import unittest
from spark_bridge import MODEL, validate_context


def valid_request():
    return {'model': MODEL, 'stream': False, 'messages': [
        {'role': 'system', 'content': 'caller text is ignored'},
        {'role': 'user', 'content': json.dumps({
            'resident': {'id': 2, 'name': '伯恩', 'personality': '节俭工匠'},
            'available_wood': 30,
            'available_plots': [{'id': 2, 'wood_cost': 6, 'description': '紧凑小屋'}]})}]}


class BridgeBoundary(unittest.TestCase):
    def test_forwards_only_game_fields(self):
        request = valid_request()
        data = json.loads(request['messages'][1]['content'])
        data['command'] = 'must never be forwarded'
        request['messages'][1]['content'] = json.dumps(data)
        result = validate_context(request)
        self.assertEqual(result['resident']['name'], '伯恩')
        self.assertNotIn('command', result)

    def test_no_model_substitution(self):
        request = valid_request(); request['model'] = 'another-model'
        with self.assertRaises(ValueError): validate_context(request)

    def test_rejects_invalid_game_data(self):
        original = json.loads(valid_request()['messages'][1]['content'])
        changes = [lambda x: x.update(available_wood=-1),
                   lambda x: x['resident'].update(id=True),
                   lambda x: x['resident'].update(personality='x'*81),
                   lambda x: x['available_plots'][0].update(id=3),
                   lambda x: x['available_plots'][0].update(wood_cost=0),
                   lambda x: x['available_plots'].append(x['available_plots'][0].copy()),
                   lambda x: x.update(available_plots=[])]
        for change in changes:
            data = copy.deepcopy(original); change(data)
            request = valid_request(); request['messages'][1]['content'] = json.dumps(data)
            with self.subTest(data=data), self.assertRaises(ValueError): validate_context(request)


if __name__ == '__main__': unittest.main()
