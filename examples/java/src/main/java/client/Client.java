package client;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.List;
import java.util.ArrayList;
import java.util.Random;
import org.json.simple.JSONObject;
import org.json.simple.JSONArray;

/**
 * A basic client that sends queries to the Clipper REST frontend. 
 */
public class Client {

    public static void main(String[] args) {
        Client client = new Client();
        try {
            client.add_mnist_app("localhost");
            Thread.sleep(1000); // 1 second
            while (true) {
                client.predict("localhost", 4, client.randomList(1000));
            }
        } catch (Exception e) {
            System.out.println(e);
        }
    }
    
    @SuppressWarnings("unchecked")
    private void add_mnist_app(String host) throws Exception {
        JSONObject json = new JSONObject();
        json.put("name", "example_app");
        JSONObject models = new JSONObject();
        models.put("model_name", "example_model");
        models.put("model_version", 1);
        JSONArray cand_models = new JSONArray();
        cand_models.add(models);
        json.put("candidate_models", cand_models);
        json.put("input_type", "doubles");
        json.put("output_type", "doubles");
        json.put("selection_policy", "EXP3");
        json.put("latency_slo_micros", 20000);
        sendPost("http://" + host + ":1338/admin/add_app", json);
    }
    
    @SuppressWarnings("unchecked")
    private void predict(String host, int uid, List<Double> x) throws Exception {
        JSONObject json = new JSONObject();
        json.put("uid", uid);
        json.put("input", x);
        sendPost("http://" + host + ":1337/example_app/predict", json);
    }
    
    private void sendPost(String url, JSONObject json) throws Exception {
        long startTime = System.currentTimeMillis();
        HttpURLConnection con = (HttpURLConnection) new URL(url).openConnection();
        con.setRequestMethod("POST");
        con.setDoOutput(true);
        con.setRequestProperty("Content-Type", "application/json");
        OutputStreamWriter out = new OutputStreamWriter(con.getOutputStream()); 
        out.write(json.toJSONString());
        out.flush();
        out.close();
        BufferedReader in = new BufferedReader(
            new InputStreamReader(con.getInputStream()));
        String inputLine;
        StringBuffer response = new StringBuffer();
        while ((inputLine = in.readLine()) != null) {
            response.append(inputLine);
        }
        in.close();
        long latency = System.currentTimeMillis() - startTime;
        System.out.println("'" + response.toString() + "', " + latency + " ms");
    }
    
    private List<Double> randomList(int n) {
        ArrayList<Double> rtn = new ArrayList<>();
        Random random = new Random();
        for (int i = 0; i < n; ++i) {
            rtn.add(random.nextDouble());
        }
        return rtn;
    }
}
