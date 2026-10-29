
<table>
<thead>
  <tr>
    <th style="width: 30%">发布日期</th>
    <th style="width: 70%">修订记录</th>
  </tr>
  </thead>
  <tbody>
  <tr>
    <td>2026-03-30</td>
    <td>第十二次正式发布。</br>发布OmniOperator 2.1.0：</br><li>新增InsertIntoHadoopFsRelationCommand支持插入HDFS、WriteFile支持ORC写入、Window支持Array数据分段、FileSourceScanExec支持Array数据读取、LocalLimitExec支持Array数据截取。</li><li>新增支持datediff、pmod、charTypeWriteSideCheck、least、concat_ws、get_json_object表达式。</li></td>
  </tr>
  <tr>
    <td>2025-12-30</td>
    <td>第十一次正式发布。</br>发布OmniOperator 2.0.0：</br>新增适配Spark的适配层Gluten 1.3版本；SparkExtension新增支持concat_ws、regexp、regexp_replace、trim和floor表达式。</td>
  </tr>
  <tr>
    <td>2025-06-30</td>
    <td>第十次正式发布。</br><li>Spark 3.3.1新增Parquet格式支持列式写入。</li> <li>新增支持操作系统CentOS 7.9。</li></td>
  </tr>
  <tr>
    <td>2025-03-30</td>
    <td>第九次正式发布。</br><li>OmniOperator算子加速适配Hive引擎。</li><li> 新增支持Spark 3.4.3、Spark 3.5.2版本。</li></td>
  </tr>
  <tr>
    <td>2024-12-30</td>
    <td>第八次正式发布。</br><li>OmniOperator算子加速适配Hive引擎。 </li><li>新增算子加速支持算子、表达式详情。</li></td>
  </tr>
  <tr>
    <td>2024-09-30</td>
    <td>第七次正式发布。</br><li>新增支持greatest/contains表达式，支持含标量子查询表达式的filter算子不回退。 </li><li>优化TableScan、HashJoin、Shuffle、RollUp算子性能。</li><li> 优化OmniOperator算子加速部署形态，支持Yarn模式下自动部署。</li></td>
  </tr>
  <tr>
    <td>2024-06-30</td>
    <td>第六次正式发布。</br><li>OmniOperator算子加速内存优化，支持大宽表查询。</li><li> 新增支持not表达式、支持Spark insert场景下的AnsiCast表达式。</li><li> 新增对Hive引擎的支持，TPC-DS性能提升20%。</li></td>
  </tr>
  <tr>
    <td>2024-03-30</td>
    <td>第五次正式发布。</br><li>新增支持Spark 3.3.1版本，TPC-DS性能提升30%。</li><li>新增NullType数据类型，SubqueryBroadcast/Coalesce/Limit算子，instr/startswith/endswith函数，安全集群下ORC和Parquet格式数据处理，Window和HashAggregator算子的Spill功能，HashJoin、Sort和Aggregator算子的NEON指令优化。</li></td>
  </tr>
  <tr>
    <td>2023-09-30</td>
    <td>第四次正式发布。</br><li>Spark 3.1.1版本TPC-DS性能提升30%。</li><li> 新增支持TopNSort、SortMergeJoin和Sort融合算子。</li><li> 新增支持Parquet数据格式处理。</li></td>
  </tr>
  <tr>
    <td>2023-06-30</td>
    <td>第三次正式发布。</br>Spark 3.1.1版本支持成功运行TPC-DS数据集，性能提升20%。</td>
  </tr>
  <tr>
    <td>2022-12-30</td>
    <td>第二次正式发布。</br><li>新增支持Spark 3.1.1版本，成功运行大数据联盟的10条SQL标准测试用例。</li><li> 新增支持UDF功能。</li></td>
  </tr>
  <tr>
    <td>2022-06-30</td>
    <td>第一次正式发布。</br>OmniRuntime OmniOperator算子加速特性通过使用一个基础架构支撑不同引擎（例如Spark），减少重复优化工作，从而释放通用、异构算力，有效推动鲲鹏生态。</td>
  </tr>
  </tbody>
</table>
