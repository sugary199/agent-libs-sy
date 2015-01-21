package com.sysdigcloud.sdjagent;

import com.fasterxml.jackson.databind.ObjectMapper;
import org.yaml.snakeyaml.Yaml;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.logging.Logger;

/**
 * Created by luca on 12/01/15.
 */
public class Config {
    private Map<String, Object> conf;
    private Yaml yaml;
    private static final ObjectMapper mapper = new ObjectMapper();
    private final static Logger LOGGER = Logger.getLogger(Config.class.getName());

    public Config() throws FileNotFoundException {
        // Load config from file
        File conf_file = new File("dragent.yaml");
        if (!conf_file.exists())
        {
            conf_file = new File("/opt/draios/bin/dragent.yaml");
        }
        FileInputStream conf_file_stream = new FileInputStream(conf_file);
        yaml = new Yaml();
        conf = (Map<String, Object>)((Map<String, Object>) yaml.load(conf_file_stream)).get("jmx");
    }

    public Map<String, String> getPatterns()
    {
        Map<String, String> queries = new HashMap<String, String>();
        for ( String key : conf.keySet())
        {
            if ( ! key.equals("default"))
            {
                queries.put(((Map<String, String>) conf.get(key)).get("pattern"), key);
            }
        }
        return queries;
    }

    public List<BeanQuery> getDefaultBeanQueries() {
        List<BeanQuery> beanQueryList = new ArrayList<BeanQuery>();
        for (Object bean : (List<Object>) conf.get("default")) {
            beanQueryList.add(mapper.convertValue(bean, BeanQuery.class));
        }
        return beanQueryList;
    }

    public List<BeanQuery> getBeanQueries(String name) {
        Map<String, Object> queryEntry = (Map<String, Object>)conf.get(name);
        List<BeanQuery> beanQueryList = new ArrayList<BeanQuery>();
        if (queryEntry != null) {
            List beansList = mapper.convertValue(queryEntry.get("beans"), List.class);
            if (beansList != null) {
                for (Object beanQuery : beansList) {
                    // FIXME: what happens if query has an invalid value?
                    beanQueryList.add(mapper.convertValue(beanQuery, BeanQuery.class));
                }
            }
        }
        return beanQueryList;
    }
}
