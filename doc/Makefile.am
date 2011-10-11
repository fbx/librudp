
if HAVE_MKDOC

html-local:
	$(mkdoc) \
		--conf-file $(srcdir)/mkdoc.conf \
		--doc-path $(srcdir)/. \
		-I $(top_srcdir)/include \
		rudp/compiler.h rudp/error.h rudp/time.h		\
		rudp/list.h rudp/rudp.h rudp/packet.h \
		rudp/address.h \
		rudp/endpoint.h rudp/peer.h rudp/client.h	\
		rudp/server.h


clean-local:
	-rm -r html

endif
