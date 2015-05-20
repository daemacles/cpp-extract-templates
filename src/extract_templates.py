#!/usr/bin/env python2.7

from __future__ import print_function
import asciitree
import sys

# this is here to allow 'run -i' from within ipython
try:
    if clang.cindex:
        pass
except NameError:
    import clang.cindex
    clang.cindex.Config.set_library_file('/usr/lib/x86_64-linux-gnu/libclang-3.5.so.1')

index = clang.cindex.Index.create()
index_options = ['-x', 'c++',            # force C++ mode
                 '-std=c++11',           # using C++11
                 '-D__CODE_GENERATOR__'  # 
                ]

if len(sys.argv) < 2:
    print("usage: {} <file.cc>".format(sys.argv[0]))
    sys.exit(-1)

target_file = sys.argv[1]

translation_unit = index.parse(target_file, index_options)
cursor = translation_unit.cursor

def get_main(cursor, l):
    text = cursor.spelling or cursor.displayname
    if text == 'main':
        l.append(cursor)
    else:
        [get_main(child, l) for child in cursor.get_children() if target_file
        in child.location.file.name]

def traverse(cursor, function, stack=[]):
    stack.append(cursor)
    function(stack)
    for child in cursor.get_children():
        if target_file in child.location.file.name:
            traverse(child, function, stack)
    stack.pop()

template_vars = []
template_refs = []
def find_template_stuff(stack, verbose=False):
    cursor = stack[-1]
    if cursor.kind == clang.cindex.CursorKind.TEMPLATE_REF:
        template_refs.append(cursor)
        var = stack[-2]
        template_vars.append(var)
        if verbose:
            print("{} : {}".format(var.spelling or var.displayname,
                                   var.get_definition().type.spelling))

            for i in stack:
                print(print_node(i), '-->', end=' ' )
            print()

def node_children(node):
    return [c for c in node.get_children()] # if target_file in c.location.file.name]

def print_node(node):
    text = node.spelling or node.displayname
    kind = str(node.kind)[11:]
    type_kind = str(node.type.kind)[9:]
    return '{} {} {}'.format(kind, type_kind, text)

def expand_tree(cursor):
    tree = [cursor]
    for child in cursor.get_children():
        tree.append(expand_tree(child))
    return tree

class ExtractTemplates:
    def __init__(self):
        self.templates = set()
        self.includes = set()

    def __call__(self, stack):
        cursor = stack[-1]
        if cursor.kind == clang.cindex.CursorKind.TEMPLATE_REF:
            source = cursor.referenced.location.file.name
            if target_file not in source:
                var = stack[-2]
                template = var.get_definition().type.spelling
                self.templates.add(template)
                self.includes.add(source)

#print(asciitree.draw_tree(translation_unit.cursor, node_children, print_node))

# main = []
# get_main(cursor, main)
# main = main[0]
# 
traverse(cursor, find_template_stuff)

template_extractor = ExtractTemplates()
traverse(cursor, template_extractor)

print("Found the following class templates with definitions outside of {}:".format(target_file))
for template in template_extractor.templates:
    print("  {}".format(template))
print()

print("Saving template instantations to template_instances.cc")
with open('template_instances.cc', 'w') as f:
    for include in template_extractor.includes:
        f.write('#include "{}"\n'.format(include))
    for template in template_extractor.templates:
        f.write("template class {};\n".format(template))

print("Saving extern template declarations to extern_templates.hpp")
with open('extern_templates.hpp', 'w') as f:
    f.write("#pragma once\n")
    for template in template_extractor.templates:
        f.write("extern template class {};\n".format(template))



