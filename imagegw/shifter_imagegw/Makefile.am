shifter_imagegw_PYTHON = auth.py \
				converters.py \
				dockerhub.py \
				dockerv2.py \
				imagemngr.py \
				imageworker.py \
				__init__.py \
				munge.py \
				transfer.py

shifter_imagegwdir = $(pyexecdir)/shifter_imagegw

__init__.py:
	cat __init__.py.in | sed 's|!sysconfdir!|$(sysconfdir)|g' > __init__.py

clean-local: clean-local-check
clean-local-check:
	-rm __init__.py
