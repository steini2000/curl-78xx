package com.pcs.curl78xx;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.logging.Logger;

import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.annotation.WebServlet;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

@WebServlet("/post")
public class Upstream extends HttpServlet {

	private static final String LGPRE = "[curl78xx-us] ";
	private static final Logger LOG = Logger.getLogger("com.pcs.curl78xx");

	private static final long serialVersionUID = 1L;

	public Upstream() {
		super();
		LOG.info(LGPRE + "CREATE Upstream");
	}

	@Override
	public void init(ServletConfig config) throws ServletException {
		super.init(config);
		LOG.info(LGPRE + "init Upstream");
	}

	@Override
	public void destroy() {
		super.destroy();
	}

	protected void doPost(HttpServletRequest request, HttpServletResponse response)
			throws ServletException, IOException {

		LOG.info(LGPRE + "Received upstream request");

		synchronized (this) {
			LOG.info(LGPRE + "Handling upstream request");
			postImplementation(request, response);
		}
	}

	private void postImplementation(HttpServletRequest request, HttpServletResponse response) throws IOException {

		//remove servlet default headers
		response.reset();
		
		request204(request, response);
		//handleRequest(request, response);
	}
	
	@SuppressWarnings("unused")
	private void handleRequest(HttpServletRequest request, HttpServletResponse response) throws IOException {

		byte[] bdata = getBytesFromInputStream(request.getInputStream());
		int blength = bdata.length;
		LOG.info(LGPRE + "Upstream data length = " + blength);

		response.setStatus(HttpServletResponse.SC_OK);
	}

	private void request204(HttpServletRequest request, HttpServletResponse response) {
		LOG.info(LGPRE + "Sending 204 Error Reply");
		//drain(request);
		response.setStatus(HttpServletResponse.SC_NO_CONTENT);
	}

	@SuppressWarnings("unused")
	private void drain(HttpServletRequest request) {
			byte[] bdata;
			try {
				bdata = getBytesFromInputStream(request.getInputStream());
				int blength = bdata.length;
				LOG.info(LGPRE + "Upstream drained data length = " + blength);
			} catch (IOException e) {
				LOG.warning(LGPRE + "IOException while draining");
			}
	}

	public static byte[] getBytesFromInputStream(InputStream is) throws IOException {
		try (ByteArrayOutputStream os = new ByteArrayOutputStream();) {
			byte[] buffer = new byte[0xFFFF];

			for (int len; (len = is.read(buffer)) != -1;)
				os.write(buffer, 0, len);

			os.flush();

			return os.toByteArray();
		}
	}
}
