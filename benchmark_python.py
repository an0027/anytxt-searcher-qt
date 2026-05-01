"""
Python benchmark equivalent for comparison with C++ version.
Tests the same indexing and search performance.
"""
import os
import sys
import time
import random
import tempfile
import shutil
import xapian

# Parameters
NUM_FILES = 1000
NUM_SEARCHES = 50
BATCH_SIZE = 500

def generate_docs(doc_dir, num_files):
    keywords = ["人工智能", "机器学习", "深度学习", "自然语言处理", "计算机视觉",
                "数据挖掘", "大数据", "云计算", "区块链", "物联网",
                "Xapian", "Qt", "C++", "Python", "性能优化",
                "AnyTXT", "全文搜索", "索引", "检索", "文档分析"]
    
    os.makedirs(doc_dir, exist_ok=True)
    total_chars = 0
    
    for i in range(num_files):
        filepath = os.path.join(doc_dir, f"doc_{i:06d}.txt")
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(f"文档编号: {i}\n标题: 测试文档 {i}\n\n")
            for p in range(random.randint(3, 8)):
                f.write(f"这是第{p}段。")
                for k in range(random.randint(1, 3)):
                    ki = random.randint(0, len(keywords) - 1)
                    f.write(f"{keywords[ki]}是重要的技术方向。")
                f.write("\n")
        total_chars += os.path.getsize(filepath)
    
    return total_chars

def index_docs(db_path, doc_dir, num_files, batch_size):
    db = xapian.WritableDatabase(db_path, xapian.DB_CREATE_OR_OPEN)
    stemmer = xapian.Stem("english")
    termgen = xapian.TermGenerator()
    termgen.set_stemmer(stemmer)
    termgen.set_flags(xapian.TermGenerator.FLAG_CJK_NGRAM)
    
    for i in range(num_files):
        filepath = os.path.join(doc_dir, f"doc_{i:06d}.txt")
        
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        fi = os.stat(filepath)
        doc = xapian.Document()
        termgen.set_document(doc)
        termgen.index_text(content)
        
        # Values for sorting
        doc.add_value(0, f"{int(fi.st_mtime):020d}")
        doc.add_value(1, f"{fi.st_size:020d}")
        
        # Boolean terms
        doc.add_boolean_term("XTYPEtext/plain")
        doc.add_boolean_term("XEXTtxt")
        
        uterm = f"PATH{filepath}"
        db.replace_document(uterm, doc)
        
        if (i + 1) % batch_size == 0 or i == num_files - 1:
            db.commit()
    
    db.close()
    return True

def search(db_path, queries):
    db = xapian.Database(db_path)
    qp = xapian.QueryParser()
    stemmer = xapian.Stem("english")
    qp.set_stemmer(stemmer)
    qp.set_stemming_strategy(xapian.QueryParser.STEM_SOME)
    qp.set_database(db)
    
    flags = xapian.QueryParser.FLAG_DEFAULT | xapian.QueryParser.FLAG_CJK_NGRAM
    
    total = 0
    for query_str in queries:
        query = qp.parse_query(query_str, flags)
        enquire = xapian.Enquire(db)
        enquire.set_query(query)
        mset = enquire.get_mset(0, 20)
        total += mset.get_matches_estimated()
    
    db.close()
    return total

if __name__ == "__main__":
    print(f"\n=== Python Benchmark (xapian {xapian.__version__ if hasattr(xapian, '__version__') else '?'}) ===")
    print(f"Files: {NUM_FILES} | Searches: {NUM_SEARCHES} | Batch: {BATCH_SIZE}")
    
    test_dir = tempfile.mkdtemp(prefix="anytxt_py_bench")
    index_dir = os.path.join(test_dir, "index")
    doc_dir = os.path.join(test_dir, "docs")
    
    # Generate docs
    print("\n--- Generating documents ---")
    t0 = time.time()
    total_chars = generate_docs(doc_dir, NUM_FILES)
    gen_time = (time.time() - t0) * 1000
    print(f"Generation: {gen_time:.0f}ms | Data: {total_chars} bytes")
    
    # Index
    print(f"\n--- Indexing {NUM_FILES} documents ---")
    t0 = time.time()
    index_docs(index_dir, doc_dir, NUM_FILES, BATCH_SIZE)
    index_time = (time.time() - t0) * 1000
    docs_per_sec = NUM_FILES * 1000.0 / max(index_time, 1)
    print(f"Index time: {index_time:.0f}ms ({docs_per_sec:.1f} docs/sec)")
    
    # Search
    keywords = ["人工智能", "机器学习", "深度学习", "自然语言处理", "计算机视觉",
                "数据挖掘", "大数据", "云计算", "区块链", "物联网",
                "Xapian", "Qt", "C++", "Python", "性能优化",
                "AnyTXT", "全文搜索", "索引", "检索", "文档分析"]
    
    queries = [keywords[i % len(keywords)] for i in range(NUM_SEARCHES)]
    
    print(f"\n--- Searching {NUM_SEARCHES} queries ---")
    t0 = time.time()
    total = search(index_dir, queries)
    search_time = (time.time() - t0) * 1000
    avg_ms = search_time / max(NUM_SEARCHES, 1)
    qps = NUM_SEARCHES * 1000.0 / max(search_time, 1)
    print(f"Search time: {search_time:.0f}ms")
    print(f"Avg search: {avg_ms:.2f}ms ({qps:.1f} qps)")
    print(f"Total matches: {total}")
    
    print(f"\n=== Python Summary ===")
    print(f"Index: {index_time:.0f}ms ({docs_per_sec:.1f} docs/sec)")
    print(f"Avg search: {avg_ms:.2f}ms")
    est_min = (600000.0 / docs_per_sec) / 60.0
    print(f"\n=== 600K Files Estimate ===")
    print(f"Est. index time: {est_min:.1f} minutes")
    
    # Cleanup
    shutil.rmtree(test_dir)
