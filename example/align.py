#!/usr/bin/env python

import argparse
import fileinput
import logging

import pyrapi

logging.basicConfig(level=logging.INFO)
_log = logging.getLogger('align')

def parse_args(args=None):
    parser = argparse.ArgumentParser()
    parser.add_argument('ref')
    parser.add_argument('input', nargs='*')
    parser.add_argument('--se', action="store_true", default=False)
    options = parser.parse_args(args)
    return options

def main(argv=None):
    options = parse_args(argv)
    plugin = pyrapi.load_aligner('rapi_bwa')

    opts = plugin.opts()
    plugin.init(opts)
    _log.info("Using the %s aligner plugin, version %s", plugin.aligner_name(), plugin.aligner_version())


    _log.info("Loading reference %s", options.ref)
    ref = plugin.ref(options.ref)
    _log.info("Reference loaded")

    pe = not options.se
    _log.info("in %s mode", 'paired-end' if pe else 'single-end')

    batch = plugin.read_batch(2 if pe else 1)

    _log.info('input: %s', options.input)
    _log.info('reading data')
    for line in fileinput.input(files=options.input):
        row = line.rstrip('\n').split('\t')
        batch.append(row[0], row[1], row[2], plugin.QENC_SANGER)
        if pe:
            batch.append(row[0], row[3], row[4], plugin.QENC_SANGER)

    _log.info('Loaded %s reads', len(batch))

    _log.info("aligning...")
    aligner = plugin.aligner(opts)
    aligner.align_reads(ref, batch)
    _log.info("finished aligning")

    _log.info("Here's the output")
    for idx, fragment in enumerate(batch):
        _log.info('fragment')
        assert len(fragment) == 2
        sam = plugin.format_sam(batch, idx)
        print sam

    ref.unload()

if __name__ == '__main__':
    main()
