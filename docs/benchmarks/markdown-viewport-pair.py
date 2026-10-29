#!/usr/bin/env python3
"""Build paired header benchmarks against one Release core; Ninja/compile_commands required."""
import argparse, json, os, shlex, statistics, subprocess, tempfile
from pathlib import Path
p = argparse.ArgumentParser()
p.add_argument('--build', type=Path, required=True)
p.add_argument('--font', type=Path, required=True)
p.add_argument('--rounds', type=int, default=3)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
build = a.build.resolve()
font = a.font.resolve()
unit = root / 'tests/unit/markdown_viewport.cpp'
helper = unit.read_text().split('int main(', 1)[0]
main = 'int main() {\n const auto* font=std::getenv("EUI_TEXT_TEST_FONT");\n core::TextPrimitive::setDefaultFontFiles(font ? font : "/tmp/mirage-eui-markdown-20261023/assets/JingNanJunJunTi-JinNanJunJunTi-Bold-2.ttf",{});\n const std::string mode=std::getenv("EUI_VIEWPORT_MODE") ? std::getenv("EUI_VIEWPORT_MODE") : "viewport";\n components::MarkdownStyle style;\n std::string paragraph;\n while(paragraph.size()<18000) paragraph += "中文 English 😀 e\\xcc\\x81 ";\n std::vector<double> values;\n for(int i=0;i<65;++i){\n  const auto source=mode=="append" ? paragraph+std::to_string(i) : paragraph;\n  const auto start=std::chrono::steady_clock::now();\n  const auto result=build(source,420,style,mode!="full",500,180,false);\n  if(i>=5)values.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());\n  if(i==64)std::cout<<"{\\"mode\\":\\""<<mode<<"\\",\\"nodes\\":"<<result.nodes<<",";\n }\n const auto raw=values;std::sort(values.begin(),values.end());\n std::cout<<"\\"p50_ms\\":"<<values[30]<<",\\"p95_ms\\":"<<values[57]<<",\\"samples_ms\\":[";\n for(std::size_t i=0;i<raw.size();++i){if(i)std::cout<<",";std::cout<<raw[i];}\n std::cout<<"]}\\n";\n}\n'
entry = next(x for x in json.loads((build / 'compile_commands.json').read_text()) if Path(x['file']) == unit)
compile_args = shlex.split(entry['command'])
link = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', 'markdown_viewport'], text=True).splitlines()[-1]
link_args = shlex.split(link.split('&&')[1])
runs = []
with tempfile.TemporaryDirectory(prefix='eui-viewport-pair-') as scratch:
    temp = Path(scratch)
    old = subprocess.check_output(['git', '-C', str(root), 'show', '22f0ca35d19c06e989d64998ce18a27101ffd55c:components/markdown.h'], text=True)
    (temp / 'baseline.h').write_text(old)
    for version in ['baseline', 'current']:
        source = helper + main
        if version == 'baseline':
            source = source.replace('"components/markdown.h"', json.dumps(str(temp / 'baseline.h'))).replace('if (sliced) md.viewport(top, height);', '(void)sliced;')
        cpp, obj, exe = [temp / (version + suffix) for suffix in ['.cpp', '.o', '']]
        cpp.write_text(source)
        command = compile_args[:]
        command[command.index('-o') + 1] = str(obj)
        command[command.index('-c') + 1] = str(cpp)
        subprocess.run(command, cwd=entry['directory'], check=True)
        command = [str(obj) if x.endswith('markdown_viewport.cpp.o') else x for x in link_args]
        command[command.index('-o') + 1] = str(exe)
        subprocess.run(command, cwd=build, check=True)
    for workload in ['steady', 'append']:
        for iteration in range(a.rounds):
            for version in (['baseline', 'current'] if iteration % 2 == 0 else ['current', 'baseline']):
                mode = 'append' if workload == 'append' else 'full' if version == 'baseline' else 'viewport'
                env = dict(os.environ, EUI_TEXT_TEST_FONT=str(font), EUI_VIEWPORT_MODE=mode)
                rss = temp / 'rss'
                result = subprocess.check_output(['/usr/bin/time', '-f', '%M', '-o', str(rss), str(temp / version)], env=env, text=True)
                runs.append(dict(workload=workload, iteration=iteration, version=version, rss_kib=int(rss.read_text()), **json.loads(result)))
print(json.dumps(dict(font=str(font), runs=runs), indent=2))
